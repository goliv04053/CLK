//
//  cpu_tests.cpp
//  Clock Signal GTK frontend: processor conformance tests for Linux.
//
//  The upstream conformance tests live in OSBindings/Mac/Clock SignalTests and run
//  only from Xcode. This runner drives the same third-party test programs against the
//  same all-RAM processor harnesses, so that a Linux (GCC) build can be checked too.
//
//  Usage: clksignal-cpu-tests [path to "Clock SignalTests"] [test name ...]
//

#include "Processors/6502/AllRAM/6502AllRAM.hpp"
#include "Processors/Z80/AllRAM/Z80AllRAM.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

std::filesystem::path tests_root;

std::vector<uint8_t> load(const std::filesystem::path &path) {
	std::ifstream file(tests_root / path, std::ios::binary);
	if(!file) return {};
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

struct Result {
	bool passed = false;
	std::string detail;
};

// MARK: - 6502 family.

using Type = CPU::MOS6502Esque::Type;
using CPU::MOS6502Esque::Register;

/// Runs a Klaus Dormann test: load at 0, start at 0x400, run until the program
/// counter stops moving (every outcome, success or failure, is a branch-to-self trap).
uint16_t run_klaus_dormann(const char *file, const Type type) {
	const auto data = load(std::filesystem::path("Klaus Dormann") / file);
	if(data.empty()) return 0;

	std::unique_ptr<CPU::MOS6502::AllRAMProcessor> cpu(CPU::MOS6502::AllRAMProcessor::Processor(type));
	cpu->set_data_at_address(0, data.size(), data.data());
	cpu->set_value_of(Register::ProgramCounter, 0x400);

	while(true) {
		const auto old_pc = cpu->value_of(Register::LastOperationAddress);
		cpu->run_for(Cycles(1000));
		const auto new_pc = cpu->value_of(Register::LastOperationAddress);
		if(new_pc == old_pc) {
			cpu->run_for(Cycles(7));
			if(cpu->value_of(Register::LastOperationAddress) == old_pc) {
				return new_pc;
			}
		}
	}
}

Result klaus(const char *file, const Type type, const uint16_t success) {
	const auto trap = run_klaus_dormann(file, type);
	char detail[64];
	if(!trap) return {false, "test file not found"};
	snprintf(detail, sizeof(detail), "stopped at $%04x (success is $%04x)", trap, success);
	return {trap == success, detail};
}

Result all_suite_a() {
	const auto data = load("AllSuiteA/AllSuiteA.bin");
	if(data.empty()) return {false, "test file not found"};

	std::unique_ptr<CPU::MOS6502::AllRAMProcessor> cpu(CPU::MOS6502::AllRAMProcessor::Processor(Type::T6502));
	cpu->set_data_at_address(0x4000, data.size(), data.data());
	const uint8_t jam = CPU::MOS6502::JamOpcode;
	cpu->set_data_at_address(0x45c0, 1, &jam);
	cpu->set_value_of(Register::ProgramCounter, 0x4000);

	int iterations = 0;
	while(!cpu->is_jammed() && iterations++ < 100'000) {
		cpu->run_for(Cycles(1000));
	}
	uint8_t result;
	cpu->get_data_at_address(0x0210, 1, &result);
	char detail[64];
	snprintf(detail, sizeof(detail), "result byte $%02x (success is $ff)", result);
	return {result == 0xff, detail};
}

// MARK: - Z80.

/// Runs a CP/M program (Zexdoc/Zexall) with BDOS calls 2 and 9 trapped to collect output.
struct CPMRunner: public CPU::AllRAMProcessor::TrapHandler {
	std::unique_ptr<CPU::Z80::AllRAMProcessor> cpu{CPU::Z80::AllRAMProcessor::Processor()};
	std::string output;
	bool done = false;
	bool echo = false;

	void processor_did_trap(CPU::AllRAMProcessor &, const uint16_t address) override {
		using R = CPU::Z80::Register;
		if(address == 0) {
			done = true;
			return;
		}
		std::string text;
		switch(cpu->value_of(R::C)) {
			case 0: done = true; break;
			case 2: text = char(cpu->value_of(R::E)); break;
			case 9: {
				uint16_t pointer = cpu->value_of(R::DE);
				while(true) {
					uint8_t c;
					cpu->get_data_at_address(pointer++, 1, &c);
					if(c == '$') break;
					text += char(c);
				}
			} break;
			default: break;
		}
		output += text;
		if(echo) {
			for(const char c: text) if(c != '\r') fputc(c, stdout);
			fflush(stdout);
		}
	}

	Result run(const char *file) {
		const auto data = load(std::filesystem::path("Zexall") / file);
		if(data.empty()) return {false, "test file not found"};

		cpu->reset_power_on();
		cpu->set_data_at_address(0x100, data.size(), data.data());
		const uint8_t bdos[] = {0xc9, 0xff, 0xff};		// RET, plus a high memtop.
		cpu->set_data_at_address(5, sizeof(bdos), bdos);
		const uint8_t reset[] = {0xc3, 0x00, 0x00};		// JP 0, so that a warm boot stays put.
		cpu->set_data_at_address(0, sizeof(reset), reset);
		cpu->set_trap_handler(this);
		cpu->add_trap_address(5);
		cpu->add_trap_address(0);
		cpu->set_value_of(CPU::Z80::Register::ProgramCounter, 0x100);

		while(!done) {
			cpu->run_for(Cycles(10'000'000));
		}

		int ok = 0, failed = 0;
		for(size_t p = output.find("OK"); p != std::string::npos; p = output.find("OK", p + 2)) ++ok;
		for(size_t p = output.find("ERROR"); p != std::string::npos; p = output.find("ERROR", p + 5)) ++failed;
		const bool complete = output.find("Tests complete") != std::string::npos;
		char detail[96];
		snprintf(detail, sizeof(detail), "%d groups OK, %d with errors%s", ok, failed, complete ? "" : ", did not complete");
		return {complete && !failed && ok == 67, detail};
	}
};

/// Runs one of Patrik Rak's ZX Spectrum Z80 tests outside of a Spectrum: the final
/// TAP block is loaded at $8000 and called; RST $10 (print a character) is trapped.
struct PatrikRakRunner: public CPU::AllRAMProcessor::TrapHandler, public CPU::Z80::AllRAMProcessor::PortAccessDelegate {
	std::unique_ptr<CPU::Z80::AllRAMProcessor> cpu{CPU::Z80::AllRAMProcessor::Processor()};
	std::string output;
	bool done = false;

	uint8_t z80_all_ram_processor_input(uint16_t) override { return 191; }

	void processor_did_trap(CPU::AllRAMProcessor &, const uint16_t address) override {
		if(address == 0x7003) {
			done = true;
			return;
		}
		auto c = uint8_t(cpu->value_of(CPU::Z80::Register::A));
		if((c < 32 && c != 13) || c >= 127) c = 32;
		output += c == 13 ? '\n' : char(c);
	}

	Result run(const char *file) {
		const auto data = load(std::filesystem::path("Patrik Rak Z80 Tests") / file);
		if(data.empty()) return {false, "test file not found"};

		size_t pointer = 0, final_block = 0;
		while(pointer + 1 < data.size()) {
			const size_t size = data[pointer] | (data[pointer + 1] << 8);
			final_block = pointer + 2;
			pointer += 2 + size;
		}

		cpu->reset_power_on();
		cpu->set_port_access_delegate(this);
		cpu->set_data_at_address(0x8000, data.size() - final_block - 1, &data[final_block + 1]);
		const uint8_t ret = 0xc9;
		cpu->set_data_at_address(0x0010, 1, &ret);		// RST $10: print.
		cpu->set_data_at_address(0x1601, 1, &ret);		// Channel open.
		const uint8_t harness[] = {0xcd, 0x00, 0x80, 0xc3, 0x03, 0x70};	// CALL $8000; JP $7003.
		cpu->set_data_at_address(0x7000, sizeof(harness), harness);
		cpu->set_trap_handler(this);
		cpu->add_trap_address(0x0010);
		cpu->add_trap_address(0x7003);
		cpu->set_value_of(CPU::Z80::Register::ProgramCounter, 0x7000);

		while(!done) {
			cpu->run_for(Cycles(10'000'000));
		}

		const bool passed = output.find("Result: all tests passed.") != std::string::npos;
		std::string detail;
		const auto result = output.rfind("Result:");
		if(result != std::string::npos) {
			detail = output.substr(result, output.find('\n', result) - result);
		}
		if(!passed) {
			// List the failing test names.
			size_t line_start = 0;
			while(line_start < output.size()) {
				const auto line_end = output.find('\n', line_start);
				const auto line = output.substr(line_start, line_end - line_start);
				if(line.find("FAILED") != std::string::npos) detail += "\n    " + line;
				if(line_end == std::string::npos) break;
				line_start = line_end + 1;
			}
		}
		return {passed, detail};
	}
};

Result patrik_rak(const char *file) {
	PatrikRakRunner runner;
	return runner.run(file);
}

Result zex(const char *file, bool echo) {
	CPMRunner runner;
	runner.echo = echo;
	return runner.run(file);
}

}

int main(int argc, char *argv[]) {
	tests_root = argc > 1 ? argv[1] : "OSBindings/Mac/Clock SignalTests";
	bool verbose = false;

	const std::vector<std::pair<std::string, std::function<Result()>>> tests = {
		{"klaus-6502",			[] { return klaus("6502_functional_test.bin", Type::T6502, 0x3399); }},
		{"klaus-6502-on-65c02",	[] { return klaus("6502_functional_test.bin", Type::TWDC65C02, 0x3399); }},
		{"klaus-6502-on-65816",	[] { return klaus("6502_functional_test.bin", Type::TWDC65816, 0x3399); }},
		{"klaus-65c02",			[] { return klaus("65C02_extended_opcodes_test.bin", Type::TWDC65C02, 0x24f1); }},
		{"klaus-65c02-on-65816",[] { return klaus("65C02_no_Rockwell_test.bin", Type::TWDC65816, 0x11e0); }},
		{"allsuitea",			all_suite_a},
		{"z80doc",				[] { return patrik_rak("z80doc.tap"); }},
		{"z80docflags",			[] { return patrik_rak("z80docflags.tap"); }},
		{"z80flags",			[] { return patrik_rak("z80flags.tap"); }},
		{"z80full",				[] { return patrik_rak("z80full.tap"); }},
		{"z80ccf",				[] { return patrik_rak("z80ccf.tap"); }},
		{"z80memptr",			[] { return patrik_rak("z80memptr.tap"); }},
		{"zexdoc",				[&] { return zex("zexdoc.com", verbose); }},
		{"zexall",				[&] { return zex("zexall.com", verbose); }},
	};

	std::vector<std::string> selected;
	for(int c = 2; c < argc; c++) {
		if(std::string(argv[c]) == "-v") verbose = true;
		else selected.push_back(argv[c]);
	}

	int failures = 0;
	for(const auto &[name, test]: tests) {
		if(!selected.empty() && std::find(selected.begin(), selected.end(), name) == selected.end()) continue;
		printf("%-22s ", name.c_str());
		fflush(stdout);
		const auto start = std::chrono::steady_clock::now();
		const auto result = test();
		const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		printf("%s  %s  (%.1fs)\n", result.passed ? "PASS" : "FAIL", result.detail.c_str(), seconds);
		failures += !result.passed;
	}
	return failures ? 1 : 0;
}
