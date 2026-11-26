// f1: the std::filesystem operations OpenTTD uses (fileio, fios, ini, strings, signature,
// openttd): path decomposition, exists, create_directories, file_size, rename, remove,
// directory_iterator with is_directory/is_regular_file, last_write_time, weakly_canonical,
// all through the std::error_code overloads.  Works in a scratch directory below the
// current one; directory listings are sorted (readdir order is unspecified).
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

static void put(const fs::path &p, const char *text)
{
	FILE *f = fopen(p.c_str(), "w");
	if (f == nullptr) {
		printf("cannot create %s\n", p.c_str());
		return;
	}
	fputs(text, f);
	fclose(f);
}

static void list(const fs::path &dir)
{
	std::vector<std::string> names;
	std::error_code ec;
	for (const auto &e : fs::directory_iterator(dir, ec)) {
		std::string kind = e.is_directory() ? "dir" : e.is_regular_file() ? "file" : "other";
		names.push_back(e.path().filename().string() + " " + kind);
	}
	std::sort(names.begin(), names.end());
	printf("list %s:%s", dir.c_str(), ec ? " error" : "");
	for (auto &n : names) printf(" [%s]", n.c_str());
	printf("\n");
}

int main()
{
	fs::path p = "save/autosave/exit.sav";
	printf("filename %s stem %s extension %s parent %s\n", p.filename().c_str(), p.stem().c_str(),
	       p.extension().c_str(), p.parent_path().c_str());
	printf("joined %s\n", (fs::path("a") / "b" / "c.txt").c_str());

	fs::path root = "f1_scratch";
	std::error_code ec;
	fs::remove(root / "sub/deeper/x.txt", ec);
	fs::remove(root / "sub/deeper", ec);
	fs::remove(root / "sub", ec);
	fs::remove(root / "a.ini", ec);
	fs::remove(root / "b.ini", ec);
	fs::remove(root, ec);

	printf("exists before %d\n", (int)fs::exists(root, ec));
	fs::create_directories(root / "sub/deeper", ec);
	printf("create_directories %s\n", ec ? "error" : "ok");
	fs::create_directories(root / "sub/deeper", ec);
	printf("create_directories again %s\n", ec ? "error" : "ok");
	printf("exists after %d\n", (int)fs::exists(root / "sub/deeper", ec));

	put(root / "a.ini.new", "[misc]\nlanguage = english\n");
	put(root / "sub/deeper/x.txt", "x");
	printf("file_size %d\n", (int)fs::file_size(root / "a.ini.new", ec));
	fs::file_size(root / "missing", ec);
	printf("file_size missing: %s\n", ec == std::errc::no_such_file_or_directory ? "ENOENT" : "other");

	fs::rename(root / "a.ini.new", root / "a.ini", ec);
	printf("rename %s, old exists %d, new exists %d\n", ec ? "error" : "ok",
	       (int)fs::exists(root / "a.ini.new"), (int)fs::exists(root / "a.ini"));
	put(root / "b.ini", "old");
	put(root / "b.ini.new", "new contents");
	fs::rename(root / "b.ini.new", root / "b.ini", ec); // replaces, as ini.cpp relies on
	printf("rename over existing %s, size %d\n", ec ? "error" : "ok", (int)fs::file_size(root / "b.ini"));

	list(root);
	list(root / "sub");
	list(root / "nonexistent");

	auto t1 = fs::last_write_time(root / "a.ini", ec);
	printf("last_write_time %s\n", ec ? "error" : "ok");
	auto t2 = fs::last_write_time(root / "b.ini", ec);
	printf("b.ini not older than a.ini %d\n", (int)(t2 >= t1));

	fs::path canon = fs::weakly_canonical(root / "sub/../a.ini");
	printf("weakly_canonical is absolute %d, ends with %s\n", (int)canon.is_absolute(),
	       canon.filename().c_str());

	printf("remove file %d\n", (int)fs::remove(root / "sub/deeper/x.txt", ec));
	printf("remove non-empty dir %d %s\n", (int)fs::remove(root / "sub", ec), ec ? "error" : "ok");
	printf("remove missing %d %s\n", (int)fs::remove(root / "missing", ec), ec ? "error" : "ok");
	fs::remove(root / "sub/deeper", ec);
	fs::remove(root / "sub", ec);
	fs::remove(root / "a.ini", ec);
	fs::remove(root / "b.ini", ec);
	printf("remove root %d, exists %d\n", (int)fs::remove(root, ec), (int)fs::exists(root));
	return 0;
}
