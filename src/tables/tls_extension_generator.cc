// csv.cc
//
// Comma-Separated Value (CSV) format processing for IANA (and
// similar) files

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <regex>
#include <fstream>
#include <unistd.h>

#include "csv.h"

void write_preamble(const std::string &filename,
                    const std::string &preprocname,
                    std::vector<std::tuple<std::string, std::string, std::string>> file_and_class,
                    FILE *f=stdout) {

    std::time_t timenow = time(NULL);
    static char timestamp[128] = { '\0' };
    strftime(timestamp, sizeof(timestamp) - 1, "%Y-%m-%dT%H:%M:%SZ", gmtime(&timenow));
    fprintf(f,
            "// %s\n"
            "//\n"
            "// this file was autogenerated at %s\n"
            "// you should edit the source file(s) instead of this one\n"
            "//\n"
            "// source files:\n",
            filename.c_str(),
            timestamp);
    for (const auto &fc : file_and_class) {
        fprintf(f, "//     %s\n", std::get<0>(fc).c_str());
    }
    fprintf(f,
            "//\n\n"
            "#ifndef %s\n"
            "#define %s\n\n"
            "#include <unordered_map>\n\n",
            preprocname.c_str(),
            preprocname.c_str());
}

void write_postamble(const char *filename, FILE *f=stdout) {
    fprintf(f, "\n#endif // %s\n\n", filename);
}

void write_class(const std::vector<std::tuple<std::string, std::string>> &params,
                 const char *classname,
                 const char *sname,
                 std::vector<int>& extensions,
                 FILE *f=stdout) {
    
    fprintf(f,
            "class %s{\n"
            "    static std::unordered_map<int32_t, int32_t>& get_mapping_index() {\n"
            "        static std::unordered_map<int32_t, int32_t> mapping_index = {\n",
            classname);

    int index = 0;
    for (const auto& t : extensions) {
        fprintf(f,"        { %d, %d},\n", t, index);
        index++;
    }
    fprintf(f,
            "        };\n"
            "        return mapping_index;\n"
            "    }\n\n"
            "public:\n"
            "    static constexpr uint16_t include_list_len = %ld;\n\n"
            "    tls_extensions_assign() {}\n\n"
            "    static int32_t get_index(uint16_t type) {\n"
            "        static const std::unordered_map<int32_t, int32_t> &mapping_index = get_mapping_index();\n"
            "        auto it = mapping_index.find(type);\n"
            "        if (it != mapping_index.end()) {\n"
            "            return(it->second);\n"
            "        }\n"
            "        return -1;\n"
            "    }\n\n"
            "    static bool is_private_extension(uint16_t type) {\n"
            "        return(\n",
            extensions.size());

    bool first = true;
    for (const auto &p : params) {
        if (std::get<0>(p).compare("Reserved for Private Use") == 0) {
            if (first == false) {
                fprintf(f, "             || ");
            } else {
                first = false;
                fprintf(f, "             ");
            }
            std::string value = std::get<1>(p);
            size_t range_delim = value.find("-");
            if (range_delim != std::string::npos) {
                fprintf(f, "(type >= %s && type <= %s)\n", value.substr(0, range_delim).c_str(), value.substr(range_delim + 1).c_str());
            } else {
                fprintf(f, "(type == %s)\n", value.c_str());
            }
        }
    }

    fprintf(f,
            "        );\n"
            "    }\n\n"
            "    static bool is_unassigned_extension(uint16_t type) {\n"
            "        return(\n"
            );

    first = true;
    for (const auto &p : params) {
        if (std::get<0>(p).compare("Unassigned") == 0) {
            if (first == false) {
                fprintf(f, "             || ");
            } else {
                first = false;
                fprintf(f, "             ");
            }
            std::string value = std::get<1>(p);
            size_t range_delim = value.find("-");
            if (range_delim != std::string::npos) {
                fprintf(f, "(type >= %s && type <= %s)\n", value.substr(0, range_delim).c_str(), value.substr(range_delim + 1).c_str());
            } else {
                fprintf(f, "(type == %s)\n", value.c_str());
            }
        }
    }

    fprintf(f,
            "        );\n"
            "    }\n"
            "};\n\n");
}

void csv_file_add_mappings(std::vector<std::tuple<std::string, std::string>> &params,
                           std::string filename) {
    std::ifstream f(filename);
    csv::get_next_line(f);  // ignore first line
    while(f) {

        std::vector<std::string> csv_line = csv::get_next_line(f);
        if (csv_line.size() > 2) {

            std::string value{csv_line[0]};
            std::string keyword{csv_line[1]};

            if (keyword.compare("Unassigned") == 0 || keyword.compare("Reserved for Private Use") == 0) {
                params.emplace_back(keyword, value);
            }
        }
    };

}

void process_iana_csv_file(std::string filename,
                           const std::string &classname,
                           const std::string &sname,
                           FILE *outfile,
                           std::vector<int>& extensions,
                           bool verbose=false) {

    std::string altfile;
    size_t comma = filename.find(",");
    if (comma != std::string::npos) {
        altfile = filename.substr(0, comma);
        filename = filename.substr(comma+1);
    }

    std::vector<std::tuple<std::string, std::string>> params;
    csv_file_add_mappings(params, filename);
    if (altfile != "") {
        csv_file_add_mappings(params, altfile);
    }

    write_class(params, classname.c_str(), sname.c_str(), extensions, outfile);

}

void populate_include_list_extensions(std::string incl_extensions_file, std::vector<int>& extensions) {
    std::ifstream in(incl_extensions_file);
    std::string _include_extensions;

    if (std::getline(in, _include_extensions)) {
        std::istringstream include_extensions(_include_extensions);

        // Temporary string to hold each token
        std::string token;

        // Delimiter character (in this case, comma)
        char delimiter = ',';

        // Iterate through the string stream and split based on the delimiter
        while (std::getline(include_extensions, token, delimiter)) {
            size_t delimiterPos = token.find('-');

        // Check if the delimiter was found
            if (delimiterPos != std::string::npos) {
                // Extract the two substrings based on the delimiter position
                int firstPart = std::stoi(token.substr(0, delimiterPos));
                int secondPart = std::stoi(token.substr(delimiterPos + 1));

                for (int i = firstPart; i <= secondPart; i++) {
                    extensions.push_back(i);
                }
            } else {
                extensions.push_back(std::stoi(token));
            }
        }
    }

    //Ensure the extensions are sorted
    std::sort(extensions.begin(),extensions.end());
}
    
void usage(const char *progname) {
    fprintf(stderr, "usage: %s outfile=<of> include_extensions=<include_extensions_file> <infile.csv>:<classname> [ <infile.csv>:<classname> ... ]\n", progname);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

    // process command line arguments
    //
    bool verbose = false;
    std::string outfilename;
    std::string dirname;
    std::string incl_extensions_file;
    std::string class_name;

    std::vector<std::tuple<std::string, std::string, std::string>> file_and_class;
    for (int i=1; i<argc; i++) {
        std::string s(argv[i]);
        size_t colon = s.find(":");
        if (colon != std::string::npos) {
            std::string prefix{s.substr(0, colon)};
            std::string suffix{s.substr(colon+1)};
            std::string sname;
            size_t colon2 = suffix.find(":");
            if (colon2 != std::string::npos) {
                sname = suffix.substr(colon2+1);
                suffix = suffix.substr(0, colon2);
            } else {
                sname = suffix;
            }
            class_name = sname;

            file_and_class.emplace_back(prefix, suffix, sname);
        } else {
            if (std::regex_search(s, std::regex{"outfile=.*"})) {
                outfilename = s.substr(8);
            }
            if (std::regex_search(s, std::regex{"verbose=true"})) {
                verbose = true;
            }
            if (std::regex_search(s, std::regex{"dir=.*"})) {
                dirname = s.substr(4);
            }
            if (std::regex_search(s, std::regex{"include_extensions=.*"})) {
                incl_extensions_file = s.substr(19);
            }
        }
    }

    if (outfilename == "") {
        fprintf(stderr, "error: no output file specified on command line\n");
        usage(argv[0]);
    }


    //
    // Create header file first
    FILE *outfile = fopen(outfilename.c_str(), "w");

    if (dirname != "") {
        if (chdir(dirname.c_str()) != 0) {
            fprintf(stderr, "error: could not change working directory to %s\n", dirname.c_str());
            usage(argv[0]);
        }
    }

    // Get the extensions in include list
    std::vector<int> extensions;

    populate_include_list_extensions(incl_extensions_file, extensions);
    // create preprocessor names for #defines
    //
    std::string preproc{outfilename};
    std::replace(preproc.begin(), preproc.end(), '.', '_');
    std::transform(preproc.begin(), preproc.end(), preproc.begin(), ::toupper);

    // write out preambles, tables, and postamble
    //
    write_preamble(outfilename, preproc, file_and_class, outfile);
    for (const auto &fc : file_and_class) {
        process_iana_csv_file(std::get<0>(fc), std::get<1>(fc), std::get<2>(fc), outfile, extensions, verbose);
    }
    write_postamble(preproc.c_str(), outfile);
 
    return 0;
}
