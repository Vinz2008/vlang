#include "../../src/config.h"
#include "cli.h"
#include "files.h"
#include "dependencies.h"
#include <toml++/toml.h>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

enum mode {
    BUILD_MODE = -1,
    CLEAN_MODE = -2,
    INFO_MODE = -3,
    DOWNLOAD_MODE = -4,
    ADD_DEPENDENCY_MODE = -5,
};

std::string filename_config = "build.toml";

void downloadDependencies(toml::v3::table config){
    auto github_dependencies = config["dependencies"]["github"];
    if (toml::array* arr = github_dependencies.as_array()){
        arr->for_each([](auto&& dep){
            if constexpr (toml::is_string<decltype(dep)>){
            std::string dependency = *dep;
            std::string username = dependency.substr(0, dependency.find('/'));
            std::string repo_name = dependency.substr(dependency.find('/')+1, dependency.size());
            cloneGithub(username, repo_name, DEFAULT_PACKAGE_PATH);
            }
        });
    }
}

void addDependency(std::string dependency_name, toml::v3::table& config){
    auto github_dependencies = config["dependencies"]["github"];
    if (toml::array* arr = github_dependencies.as_array()){
        arr->for_each([dependency_name](auto&& dep){
            if constexpr (toml::is_string<decltype(dep)>){
                std::cout << "add dependency : " << dependency_name << std::endl;
                std::cout << "dep : " << dep << std::endl;
            }
        });
        arr->insert(arr->end(), dependency_name);
        //std::cout << config << std::endl; 
        std::ofstream configFstream(filename_config);
        configFstream << config;
        configFstream.close();
    }
}

int main(int argc, char** argv){
    enum mode modeBuild = BUILD_MODE;
    std::string src_folder = "src/";
    std::string dependency_to_add = "";
    for (int i = 0; i < argc; i++){
    std::string arg = argv[i];
    if (arg == "-f"){
    i++;
    filename_config = argv[i];
    } else if (arg == "build"){
        modeBuild = BUILD_MODE;
    } else if (arg == "clean"){
        modeBuild = CLEAN_MODE;
    } else if (arg == "info"){
        modeBuild = INFO_MODE;
    } else if (arg == "download" || arg == "update"){
        modeBuild = DOWNLOAD_MODE;
    } else if (arg == "add"){
        modeBuild = ADD_DEPENDENCY_MODE;
        i++;
        dependency_to_add = argv[i];
    }
    }
    fs::path f{ filename_config };
    if (!fs::exists(f)){
        std::cout << "Could not find " << filename_config << " file" << std::endl;
        exit(1);
    }
    auto config = toml::parse_file(filename_config);
    std::string_view src_folder_temp = config["project"]["src_folder"].value_or("");
    std::string_view type = config["project"]["type"].value_or("");
    if (src_folder_temp != ""){
        src_folder = src_folder_temp;
    }
    if (modeBuild == CLEAN_MODE){
        std::vector<std::string> PathList = getFilenamesWithExtension(".cpoint", src_folder);
        for (auto const& path : PathList){
            fs::path path_fs{ path };
            std::string object_path = path_fs.replace_extension(".o");
            std::cout << object_path << ' ';
            remove(object_path.c_str());
        }
    } else if (modeBuild == INFO_MODE){
        std::string_view project_name = config["project"]["name"].value_or("");
        std::cout << "project name : " << project_name << "\n";
        std::string_view homepage = config["project"]["homepage"].value_or("");
        std::cout << "homepage : " << homepage << "\n";
        std::string_view license = config["project"]["license"].value_or("");
        std::cout << "license : " << license << "\n";
        std::cout << "type : " << type << "\n";
        std::cout << "src folder : " << src_folder_temp << "\n";
    } else if (modeBuild == DOWNLOAD_MODE){
        downloadDependencies(config);
    } else if (modeBuild == ADD_DEPENDENCY_MODE){
        addDependency(dependency_to_add, config);
    } else if (modeBuild == BUILD_MODE) {
    downloadDependencies(config);
    std::string_view arguments = config["build"]["arguments"].value_or("");
    std::vector<std::string> PathList = getFilenamesWithExtension(".cpoint", src_folder);
    for (auto const& path : PathList){
        std::cout << path << ' ';
        compileFile("", "-no-gc" + (std::string)arguments, path);
    }
    std::cout << std::endl;
    if (type == "exe"){
    linkFiles(PathList);
    } else if (type == "library"){
        linkLibrary(PathList);
    }
    }

}
