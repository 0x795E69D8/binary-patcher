#include "crc.h"
#include "files.h"
#include <filesystem>
#include <regex>
using namespace std;

union Hash {
	uint32_t value;
	uint8_t buf[4];
};

struct Patch {
	unordered_map<string, string> config;
	unordered_map<int, vector<uint8_t>> data;
};

struct Settings {
	bool backup = true;
	bool check_pre_crc32 = false;
	Hash pre_crc32;
	bool check_post_crc32 = false;
	Hash post_crc32;
};

void read_data(Patch *p, const string &offsetString, string dataString) {
	dataString = regex_replace(dataString, regex ("\\s+"), "");
	dataString = dataString.length() % 2 == 0 ? dataString : "0" + dataString;
	int offset;
	vector<uint8_t> bytes;
	try {
		offset = stoi(offsetString, nullptr, 0);
		if (all_of(dataString.begin(), dataString.end(), ::isxdigit)) {
			for (string::size_type i = 0; i < dataString.size(); i += 2) {
				bytes.push_back(stoi(dataString.substr(i, 2), nullptr, 16));
			}
			p->data.insert(pair<int, vector<uint8_t>>(offset, bytes));
		} else {
			cerr << dataString << " is not a valid byte sequence!" << endl;
		}
	} catch (invalid_argument const& ex) {
		cerr << "Offset: " << offsetString << " is not a number!" << endl;
	}
}

void load_patch(Patch *p, char *filename) {
	int mode = -1;
	string line;
	smatch m;
	regex const cat{R"~(^\[([^\]]*)\]$)~"};
	regex const conf{R"~(^([A-Za-z0-9_]+)[ \t]*=[ \t]*(.+)[ \t]*$)~"};
	ifstream infile(filename);
	while (getline(infile, line)) {
		if (regex_match(line, m, cat)) {
			if (m[1] == "config") {
				mode = 0;
			} else if (m[1] == "data") {
				mode = 1;
			} else {
				mode = -1;
			}
		}
		if (regex_match(line, m, conf)) {
			switch (mode) {
				case 0:
					p->config.insert(pair<string, string>(m[1], m[2]));
					break;
				case 1:
					read_data(p, m[1], m[2]);
					break;
				default:
					break;
			}
		}
	}
	infile.close();
}

void apply_settings(Settings *s, Patch *p) {
	auto it = p->config.find("backup");
	s->backup = it != p->config.end() && (it->second == "false" || it->second == "0") ? false : true;
	it = p->config.find("pre_crc32");
	if (it != p->config.end()) {
		s->check_pre_crc32 = true;
		for (string::size_type i = 0; i < 8; i += 2) {
			s->pre_crc32.buf[i/2] = stoi(it->second.substr(6 - i, 2), nullptr, 16);
		}
	}
	it = p->config.find("post_crc32");
	if (it != p->config.end()) {
		s->check_post_crc32 = true;
		for (string::size_type i = 0; i < 8; i += 2) {
			s->post_crc32.buf[i/2] = stoi(it->second.substr(6 - i, 2), nullptr, 16);
		}
	}
}

bool verify_hash(char *filename, Hash *hash) {
	using namespace CryptoPP;
	CRC32 crc;
	Hash digest;
	FileSource f(filename, true, new HashFilter(crc, new ArraySink(digest.buf, CRC32::DIGESTSIZE)));
	return digest.value == hash->value;
}

void patch_file(Patch *p, char *filename) {
	ofstream outfile(filename, ios::binary | ios::in | ios::out);
	for(const auto &e : p->data) {
		outfile.seekp(e.first);
		outfile.write((char*) e.second.data(), e.second.size());
	}
	outfile.close();
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		cout << "Usage: patch <file to be patched> <patch configuration file>";
		return 1;
	}
	Patch patch;
	Settings settings;
	load_patch(&patch, argv[2]);
	apply_settings(&settings, &patch);
	if (settings.check_post_crc32 && verify_hash(argv[1], &settings.post_crc32)) {
		cout << argv[1] << " is already patched" << endl;
		return 0;
	}
	if (settings.check_pre_crc32) {
		if (!verify_hash(argv[1], &settings.pre_crc32)) {
			cout << argv[1] << " CRC32 checksum mismatch, cancelled" << endl;
			return 1;
		}
		cout << argv[1] << " CRC32 checksum valid" << endl;
	} else
		cout << "CRC32 pre-check disabled" << endl;
	if (settings.backup) {
		cout << "Backing up " << argv[1] << endl;
		try {
			filesystem::copy_file(argv[1], string (argv[1]) + ".bak", filesystem::copy_options::skip_existing);
		} catch (filesystem::filesystem_error ex) {
			// filesystem::copy_options doesn't seem to work with g++ so this is the alternative i guess
			cerr << "Backup already exists" << endl;
		}
	}
	cout << "Patching " << argv[1] << "..." << endl;
	patch_file(&patch, argv[1]);
	cout << "Patch complete";
	if (settings.check_post_crc32)
		cout << ", file " << (verify_hash(argv[1], &settings.post_crc32) ? "matches target CRC32 checksum" : "does not match target CRC32 checksum!");
	cout << endl;
	return 0;
}
