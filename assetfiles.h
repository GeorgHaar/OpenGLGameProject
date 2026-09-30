#ifndef ASSETFILES_H
#define ASSETFILES_H

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

inline bool ValidRelativeAssetPath(const std::string &name) {
    if (name.empty() || name.size() > 1024 || name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos) return false;
    for (unsigned char c : name) if (c < 32 || c == 127) return false;
    const std::filesystem::path path(name);
    if (path.is_absolute() || path.has_root_path() || path.filename().empty() ||
        path.generic_string() != name) return false;
    for (const auto &part : path) if (part == ".." || part == "." || part.empty()) return false;
    return true;
}

inline std::string LowerExtension(const std::filesystem::path &path) {
    std::string extension = path.extension().string();
    for (char &c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return extension;
}

inline bool BackgroundImageExtension(const std::filesystem::path &path) {
    const std::string extension = LowerExtension(path);
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
           extension == ".bmp" || extension == ".tga";
}

inline bool ModelFileExtension(const std::filesystem::path &path) {
    const std::string extension = LowerExtension(path);
    return extension == ".glb" || extension == ".gltf";
}

inline bool ValidModelName(const std::string &name) {
    return ValidRelativeAssetPath(name) && ModelFileExtension(name);
}

template <typename Filter>
std::vector<std::string> ListAssetFiles(const std::filesystem::path &directory, Filter accepts) {
    std::vector<std::string> names;
    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(directory,
        std::filesystem::directory_options::skip_permission_denied, error), end;
    while (!error && entry != end) {
        if (entry->is_regular_file(error)) {
            const std::string name = entry->path().lexically_relative(directory).generic_string();
            if (accepts(name)) names.push_back(name);
        }
        entry.increment(error);
    }
    std::sort(names.begin(), names.end());
    return names;
}

inline std::vector<std::string> ListBackgroundImages(const std::filesystem::path &assets) {
    return ListAssetFiles(assets / "backgrounds", [](const std::string &name) {
        return BackgroundImageExtension(name);
    });
}

inline std::vector<std::string> ListModelFiles(const std::filesystem::path &assets) {
    return ListAssetFiles(assets / "models", ValidModelName);
}

inline std::string LocateModelByFilename(const std::filesystem::path &assets, const std::string &file) {
    const std::string wanted = std::filesystem::path(file).filename().generic_string();
    std::string found;
    if (wanted.empty()) return found;
    for (const std::string &candidate : ListModelFiles(assets)) {
        if (std::filesystem::path(candidate).filename().generic_string() != wanted) continue;
        if (!found.empty()) return {};
        found = candidate;
    }
    return found;
}

inline std::string ImportBackgroundImage(const std::filesystem::path &assets,
                                        const std::string &sourceName, std::string &error) {
    error.clear();
    try {
        const auto source = std::filesystem::canonical(sourceName);
        if (!BackgroundImageExtension(source) || !std::filesystem::is_regular_file(source)) {
            error = "Bitte eine PNG-, JPG-, BMP- oder TGA-Datei waehlen.";
            return {};
        }
        std::filesystem::create_directories(assets / "backgrounds");
        const auto directory = std::filesystem::canonical(assets / "backgrounds");
        const auto relative = source.lexically_relative(directory);
        if (!relative.empty() && *relative.begin() != "..")
            return relative.generic_string();

        auto destination = directory / source.filename();
        for (int suffix = 2; std::filesystem::exists(destination); ++suffix)
            destination = directory / (source.stem().string() + "_" + std::to_string(suffix) +
                                       source.extension().string());
        std::filesystem::copy_file(source, destination);
        return destination.filename().generic_string();
    } catch (const std::filesystem::filesystem_error &e) {
        error = "Bild konnte nicht importiert werden: " + std::string(e.what());
        return {};
    }
}

#endif
