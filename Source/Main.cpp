#include "Libs/miniz/miniz.h"

#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define PATH_SEPARATOR "\\"
#else
#include <sys/stat.h>
#define PATH_SEPARATOR "/"
#endif

// Find the start offset of the ZIP portion in a hybrid exe+zip file
size_t find_zip_start_offset(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return 0;
    }

    // Read the entire file
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<unsigned char> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    file.close();

    // Search for ZIP local file header signature: 0x04034b50 (little-endian: 50 4b 03 04)
    const unsigned char zip_signature[] = {0x50, 0x4b, 0x03, 0x04};
    
    for (size_t i = 0; i <= file_size - 4; ++i) {
        if (memcmp(&buffer[i], zip_signature, 4) == 0) {
            printf("Found ZIP signature at offset: %zu\n", i);
            return i;
        }
    }

    return 0;
}

// Modify file contents (example: enable debug mode)
bool modify_file_contents(std::map<std::string, std::vector<unsigned char>>& file_contents) {
    // Example: Look for a specific file to modify
    // You can customize this based on what you need to change
    bool modified = false;
    
    for (auto& [filename, content] : file_contents) {
        // Find main.lua and inject debugger code
        if (filename == "main.lua" || filename.find("main.lua") != std::string::npos) {
            std::string content_str(content.begin(), content.end());
            
            // Check if debugger code already exists
            if (content_str.find("require(\"lldebugger\")") != std::string::npos) {
                printf("File %s already contains debugger code, skipping\n", filename.c_str());
                continue;
            }
            
            // Find "function love.run("
            size_t pos = content_str.find("function love.run(");
            if (pos != std::string::npos) {
                // Find the start of the line
                size_t line_start = pos;
                while (line_start > 0 && content_str[line_start - 1] != '\n') {
                    line_start--;
                }
                
                // Insert debugger code before love.run
                std::string debugger_code = 
                    "if os.getenv(\"LOCAL_LUA_DEBUGGER_VSCODE\") == \"1\" then\n"
                    "  require(\"lldebugger\").start()\n"
                    "end\n"
                    "\n";
                
                content_str.insert(line_start, debugger_code);
                content.assign(content_str.begin(), content_str.end());
                printf("Modified file: %s (injected lldebugger code)\n", filename.c_str());
                modified = true;
            } else {
                printf("Warning: Could not find 'function love.run(' in %s\n", filename.c_str());
            }
        }
    }
    
    return modified;
}

int main(int argc, char** argv, char** envp) {
    if (argc < 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        printf("Or drag and drop a file onto the executable.\n");
        return 1;
    }

    const char* input_file = argv[1];
    printf("Processing file: %s\n\n", input_file);

    // Step 1: Find where the ZIP portion starts in the hybrid file
    size_t zip_start_offset = find_zip_start_offset(input_file);
    if (zip_start_offset == 0) {
        printf("Warning: Could not find ZIP signature. File may be a pure ZIP (offset = 0)\n");
    }

    // Step 2: Read the EXE portion (everything before the ZIP)
    std::vector<unsigned char> exe_portion;
    std::string exe_only_file;
    if (zip_start_offset > 0) {
        std::ifstream file(input_file, std::ios::binary);
        exe_portion.resize(zip_start_offset);
        file.read(reinterpret_cast<char*>(exe_portion.data()), zip_start_offset);
        file.close();
        printf("Preserved EXE portion: %zu bytes\n\n", zip_start_offset);
        
        // Save the EXE-only copy
        std::string base_name = input_file;
        if (base_name.size() >= 4 && base_name.substr(base_name.size() - 4) == ".exe") {
            base_name = base_name.substr(0, base_name.size() - 4);
        }
        exe_only_file = base_name + ".love.exe";
        std::ofstream exe_out(exe_only_file, std::ios::binary);
        if (exe_out.is_open()) {
            exe_out.write(reinterpret_cast<const char*>(exe_portion.data()), exe_portion.size());
            exe_out.close();
            printf("Saved EXE-only copy: %s\n\n", exe_only_file.c_str());
        } else {
            printf("Warning: Failed to save EXE-only copy\n\n");
        }
    }

    // Step 3: Extract all files from the ZIP
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_file(&zip_archive, input_file, 0)) {
        printf("Failed to open zip archive: %s\n", input_file);
        return 1;
    }

    int num_files = mz_zip_reader_get_num_files(&zip_archive);
    printf("Number of files in archive: %d\n\n", num_files);

    std::map<std::string, std::vector<unsigned char>> file_contents;
    std::vector<mz_zip_archive_file_stat> file_stats;
    
    for (int i = 0; i < num_files; ++i) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            printf("Failed to get file stat for file index %d\n", i);
            continue;
        }

        size_t uncompressed_size = static_cast<size_t>(file_stat.m_uncomp_size);
        std::vector<unsigned char> buffer(uncompressed_size);

        if (!mz_zip_reader_extract_to_mem(&zip_archive, i, buffer.data(), uncompressed_size, 0)) {
            printf("Failed to extract file: %s\n", file_stat.m_filename);
            continue;
        }

        file_contents[file_stat.m_filename] = std::move(buffer);
        file_stats.push_back(file_stat);
        printf("Extracted file: %s (size: %zu bytes)\n", file_stat.m_filename, uncompressed_size);
    }

    mz_zip_reader_end(&zip_archive);
    printf("\n");

    // Step 4: Modify the file contents
    bool was_modified = modify_file_contents(file_contents);
    
    if (!was_modified) {
        printf("No modifications were made. Exiting without changes.\n");
        return 0;
    }
    printf("\n");

    // Step 4.5: Dump all modified files to "game" folder
    std::string input_path_str(input_file);
    size_t last_slash = input_path_str.find_last_of("/\\");
    std::string input_dir = (last_slash != std::string::npos) ? input_path_str.substr(0, last_slash + 1) : "";
    std::string game_dir = input_dir + "game";
    
    // Create game directory
    #ifdef _WIN32
        _mkdir(game_dir.c_str());
    #else
        mkdir(game_dir.c_str(), 0755);
    #endif
    
    // Write all modified files to the game folder
    for (const auto& [filename, content] : file_contents) {
        // Skip directory entries (they end with /)
        if (!filename.empty() && (filename.back() == '/' || filename.back() == '\\')) {
            continue;
        }
        
        std::string output_path = game_dir + PATH_SEPARATOR + filename;
        
        // Normalize path separators for current OS
        #ifdef _WIN32
            std::replace(output_path.begin(), output_path.end(), '/', '\\');
        #else
            std::replace(output_path.begin(), output_path.end(), '\\', '/');
        #endif
        
        // Create subdirectories if needed
        std::string current_path = game_dir;
        std::string remaining = filename;
        size_t pos = 0;
        while ((pos = remaining.find_first_of("/\\")) != std::string::npos) {
            std::string dir_part = remaining.substr(0, pos);
            current_path += PATH_SEPARATOR + dir_part;
            #ifdef _WIN32
                _mkdir(current_path.c_str());
            #else
                mkdir(current_path.c_str(), 0755);
            #endif
            remaining = remaining.substr(pos + 1);
        }
        
        // Write file
        std::ofstream out_file(output_path, std::ios::binary);
        if (out_file.is_open()) {
            out_file.write(reinterpret_cast<const char*>(content.data()), content.size());
            out_file.close();
            printf("Wrote to game folder: %s\n", filename.c_str());
        } else {
            printf("Warning: Failed to write file: %s\n", output_path.c_str());
        }
    }
    printf("\n");

    // Step 5: Create a new ZIP with the modified contents
    mz_zip_archive new_zip_archive;
    memset(&new_zip_archive, 0, sizeof(new_zip_archive));
    
    // Write to a memory buffer
    if (!mz_zip_writer_init_heap(&new_zip_archive, 0, 0)) {
        printf("Failed to initialize ZIP writer\n");
        return 1;
    }

    // Add all files to the new ZIP
    for (const auto& file_stat : file_stats) {
        const std::string filename = file_stat.m_filename;
        const auto& content = file_contents[filename];
        
        // Preserve original file timestamps and attributes
        if (!mz_zip_writer_add_mem_ex(&new_zip_archive, 
                                       filename.c_str(), 
                                       content.data(), 
                                       content.size(),
                                       nullptr, 0, 
                                       MZ_BEST_COMPRESSION,
                                       0, 0)) {
            printf("Failed to add file to new ZIP: %s\n", filename.c_str());
            mz_zip_writer_end(&new_zip_archive);
            return 1;
        }
        printf("Added to new ZIP: %s\n", filename.c_str());
    }

    // Finalize the ZIP archive
    void* zip_data = nullptr;
    size_t zip_size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&new_zip_archive, &zip_data, &zip_size)) {
        printf("Failed to finalize ZIP archive\n");
        mz_zip_writer_end(&new_zip_archive);
        return 1;
    }

    mz_zip_writer_end(&new_zip_archive);
    printf("\nNew ZIP size: %zu bytes\n\n", zip_size);

    // Step 6: Write the hybrid file (EXE + new ZIP)
    std::string base_name = input_file;
    if (base_name.size() >= 4 && base_name.substr(base_name.size() - 4) == ".exe") {
        base_name = base_name.substr(0, base_name.size() - 4);
    }
    std::string output_file = base_name + ".patched.exe";
    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        printf("Failed to create output file: %s\n", output_file.c_str());
        free(zip_data);
        return 1;
    }

    // Write EXE portion if it exists
    if (!exe_portion.empty()) {
        out.write(reinterpret_cast<const char*>(exe_portion.data()), exe_portion.size());
    }

    // Write new ZIP portion
    out.write(reinterpret_cast<const char*>(zip_data), zip_size);
    out.close();
    free(zip_data);

    printf("Successfully created patched file: %s\n", output_file.c_str());
    printf("Original ZIP offset: %zu bytes\n", zip_start_offset);
    printf("Original file size: %zu bytes\n", exe_portion.size() + zip_size);
    
    // Step 7: Create launch.json in .vscode folder next to the input file
    std::string vscode_dir = input_dir + ".vscode";
    std::string launch_json_path = vscode_dir + PATH_SEPARATOR "launch.json";
    
    // Create .vscode directory if it doesn't exist
    #ifdef _WIN32
        _mkdir(vscode_dir.c_str());
    #else
        mkdir(vscode_dir.c_str(), 0755);
    #endif
    
    // Escape backslashes in the path for JSON (Windows only)
    std::string escaped_output_path = output_file;
    std::string escaped_love_exe_path = exe_only_file;
    
    #ifdef _WIN32
        size_t pos = 0;
        while ((pos = escaped_output_path.find("\\", pos)) != std::string::npos) {
            escaped_output_path.replace(pos, 1, "\\\\");
            pos += 2;
        }
        
        pos = 0;
        while ((pos = escaped_love_exe_path.find("\\", pos)) != std::string::npos) {
            escaped_love_exe_path.replace(pos, 1, "\\\\");
            pos += 2;
        }
    #endif
    
    std::ofstream launch_json(launch_json_path);
    if (launch_json.is_open()) {
        launch_json << "{\n";
        launch_json << "  \"version\": \"0.2.0\",\n";
        launch_json << "  \"configurations\": [\n";
        launch_json << "    {\n";
        launch_json << "      \"name\": \"Debug Love\",\n";
        launch_json << "      \"type\": \"lua-local\",\n";
        launch_json << "      \"request\": \"launch\",\n";
        launch_json << "      \"program\": {\n";
        launch_json << "        \"command\": \"" << escaped_output_path << "\"\n";
        launch_json << "      },\n";
        launch_json << "      \"args\": [\n";
        launch_json << "        \"--disable-console\"\n";
        launch_json << "      ]\n";
        launch_json << "    },\n";
        launch_json << "    {\n";
        launch_json << "      \"name\": \"Debug Balatro Love\",\n";
        launch_json << "      \"type\": \"lua-local\",\n";
        launch_json << "      \"request\": \"launch\",\n";
        launch_json << "      \"program\": {\n";
        launch_json << "        \"command\": \"" << escaped_love_exe_path << "\"\n";
        launch_json << "      },\n";
        launch_json << "      \"args\": [\n";
        launch_json << "        \"game\",\n";
        launch_json << "        \"--disable-console\"\n";
        launch_json << "      ],\n";
        launch_json << "      \"scriptRoots\": [\n";
        launch_json << "        \"game\"\n";
        launch_json << "      ]\n";
        launch_json << "    }\n";
        launch_json << "  ]\n";
        launch_json << "}\n";
        launch_json.close();
        printf("Created launch.json: %s\n", launch_json_path.c_str());
    } else {
        printf("Warning: Failed to create launch.json\n");
    }
    
    return 0;
}