#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>

void xor1(uint8_t *bytes, size_t size)
{
	const uint8_t pattern[] = {
		0x4B, 0x47, 0x0F, 0x45,
		0x14, 0x25, 0x13, 0x2E,
		0x51, 0x2C, 0x75, 0x55,
		0x5C, 0x25, 0x6D, 0x35,
		0x1D, 0x3A, 0x22, 0x53
	};

	uint32_t eax = 0, edx = 0;

	for (uint32_t ecx = 0; ecx < size; ecx++) {
		eax = 0x66666667;
		// begin imul
		//edx:eax = ecx * eax;
		uint64_t u64 = (int64_t)ecx * (int64_t)eax;
		edx = u64 >> 32;
		eax = (uint32_t)u64;
		// end imul
		edx >>= 3; // sar
		eax = edx;
		eax >>= 0x1f; // shr
		eax += edx;
		//edx = eax*5;
		//edx += edx;
		//edx += edx;
		edx = eax * 20;
		eax = ecx;
		eax -= edx;
		uint8_t dl = pattern[eax];
		bytes[ecx] ^= dl;
	}
}

void xor2(uint8_t *bytes, size_t size)
{
	const uint8_t pattern[] = {
		0x8F, 0x48, 0x1B, 0x2E,
		0xB7, 0x2D, 0x16, 0xA7,
		0xB5, 0x18, 0x26, 0x86,
		0x18, 0x3F, 0x75, 0xB4,
		0xB9, 0x4C, 0x1A, 0x55
	};

	uint32_t eax = 0, edx = 0;

	for (uint32_t ecx = 0; ecx < size; ecx++) {
		eax = 0xcccccccd;
		// begin imul
		//edx:eax = ecx * eax;
		uint64_t u64 = (uint64_t)ecx * (uint64_t)eax;
		edx = u64 >> 32;
		eax = (uint32_t)u64;
		// end imul
		edx >>= 4; // shr
		//eax = edx*5;
		//eax += eax;
		//eax += eax;
		eax = edx * 20;
		edx = ecx;
		edx -= eax;
		uint8_t dl = pattern[edx];
		bytes[ecx] ^= dl;
	}
}

void do_read(void *buffer, size_t n, std::ifstream& file)
{
	file.read((char*)buffer, n);
	xor1((uint8_t*)buffer, n);
}

void do_write(void *buffer, size_t n, std::ofstream& file)
{
	// Don't edit the original buffer
	auto buffer2 = std::make_unique<uint8_t[]>(n);
	memcpy(buffer2.get(), buffer, n);
	xor1(buffer2.get(), n);
	file.write((char*)buffer2.get(), n);
}

std::vector<std::filesystem::path> parse_paths(uint8_t *buffer, size_t size)
{
	std::vector<std::filesystem::path> out;
	std::string s((char*)buffer, size);
	std::string delim = "\r\n";

	size_t start = 0;
	size_t end = s.find(delim);
	while (end != std::string::npos)
	{
		out.push_back(s.substr(start, end - start));
		start = end + delim.length();
		end = s.find(delim, start);
	}

	if (s.substr(start, end) != "") {
		out.push_back(s.substr(start, end));
	}
	return out;
}

void try_age_script_chara(uint8_t *buffer, size_t size)
{
	const char *name = "AGE_SCRIPT_CHARA";
	if (size <= 0x18 || memcmp(buffer, name, strlen(name)) != 0) {
		return;
	}

	size_t pos = 0x18;
	while (pos < size) {
		uint16_t elem_size;
		memcpy(&elem_size, buffer + pos, 2);
		pos += 2;
		if (pos + elem_size < size) {
			xor2(buffer + pos, elem_size);
		}
		pos += elem_size;
	}
}

int unpack(char *dat_fn)
{
	std::ifstream fin(dat_fn, std::ios_base::binary);

	uint32_t nb_files;
	do_read(&nb_files, 4, fin);
	std::cout << nb_files << " files" << std::endl;

	std::vector<std::filesystem::path> path_list;
	std::filesystem::path root = std::string(dat_fn) + ".d";
	for (size_t i = 0; i < nb_files; i++) {
		uint32_t size = 0;
		do_read(&size, 4, fin);
		if (size == 0) continue;

		auto buffer = std::make_unique<uint8_t[]>(size);
		do_read(buffer.get(), size, fin);

		std::filesystem::path path;
		if (i == 0) {
			path_list = parse_paths(buffer.get(), size);
			path = root / "__LIST__";
		}
		else {
			path = root / path_list[i];
		}
		auto path_dir = path;
		path_dir.remove_filename();
		std::filesystem::create_directories(path_dir);

		try_age_script_chara(buffer.get(), size);

		std::ofstream fout(path, std::ios_base::binary);
		fout.write((char*)buffer.get(), size);
	}

	return 0;
}

std::vector<uint8_t> read_file(std::filesystem::path path)
{
	std::ifstream file(path, std::ios_base::binary);

	file.seekg(0, std::ios_base::end);
	size_t size = (size_t)file.tellg();
	file.seekg(0, std::ios_base::beg);

	std::vector<uint8_t> data(size);
	file.read((char*)data.data(), size);

	return data;
}

int repack(char *dat_fn)
{
	std::filesystem::path root = std::string(dat_fn) + ".d";

	std::ofstream fout(dat_fn, std::ios_base::binary);

	auto path_list_txt = read_file(root / "__LIST__");
	auto path_list = parse_paths(path_list_txt.data(), path_list_txt.size());

	uint32_t nb_files = path_list.size();
	do_write(&nb_files, 4, fout);
	std::cout << nb_files << " files" << std::endl;

	for (auto it : path_list) {
		auto file = read_file(root / it);
		try_age_script_chara(file.data(), file.size());

		uint32_t size = file.size();
		do_write(&size, 4, fout);
		do_write(file.data(), file.size(), fout);
	}

	return 0;
}

int usage(char **av)
{
	std::cout << "Usage: " << av[0] << " [-x|-p] file.dat" << std::endl;
	return 1;
}

int main(int ac, char **av)
{
	if (ac != 3) {
		return usage(av);
	}
	if (av[1][0] != '-') {
		return usage(av);
	}
	if (av[1][1] == 'x') {
		return unpack(av[2]);
	}
	else if (av[1][1] == 'p') {
		return repack(av[2]);
	}
	else {
		return usage(av);
	}
}
