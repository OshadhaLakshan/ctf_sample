import re

with open("backend/src/engine.cpp", "r") as f:
    engine = f.read()

engine = engine.replace(
    'tool == "HTTP_GET"             ? std::set<std::string>{"path", "headers"}',
    'tool == "HTTP_GET"             ? std::set<std::string>{"path", "url", "headers"}'
)

engine = engine.replace(
    '''    if (tool == "HTTP_GET") {
        labOrigin();
        const std::string path =
            arguments.at("path"); // Relative origin-form path, not an arbitrary URL.
        if (path.empty() || path.size() > 2048 || path.front() != '/' || path.starts_with("//") ||
            path.find('\\\\') != std::string::npos ||
            std::any_of(path.begin(), path.end(), [](unsigned char character) {
                return character <= 32 || character == 127;
            }))
            throw std::invalid_argument(
                "Policy requires a relative HTTP path without control characters");''',
    '''    if (tool == "HTTP_GET") {
        if (!arguments.contains("path") && !arguments.contains("url"))
            throw std::invalid_argument("HTTP_GET requires path or url");'''
)

engine = engine.replace(
    '''    if (tool == "HTTP_GET") {
        httplib::Client client(
            PolicyEngine::labOrigin()); // Single literal IP origin prevents DNS rebinding and SSRF.''',
    '''    if (tool == "HTTP_GET") {
        std::string target_origin = PolicyEngine::labOrigin();
        std::string target_path = "/";
        if (arguments.contains("url")) {
            std::string url = arguments.at("url").get<std::string>();
            size_t path_pos = url.find('/', url.find("://") + 3);
            if (path_pos != std::string::npos) {
                target_origin = url.substr(0, path_pos);
                target_path = url.substr(path_pos);
            } else {
                target_origin = url;
            }
        } else if (arguments.contains("path")) {
            target_path = arguments.at("path").get<std::string>();
        }
        httplib::Client client(target_origin);'''
)

engine = engine.replace(
    '''const auto response = client.Get(arguments["path"].get<std::string>(), headers,''',
    '''const auto response = client.Get(target_path, headers,'''
)

with open("backend/src/engine.cpp", "w") as f:
    f.write(engine)

with open("backend/src/gemma.cpp", "r") as f:
    gemma = f.read()

gemma = gemma.replace(
    'Tools: HTTP_GET {path,headers?}, "',
    'Tools: HTTP_GET {url,path,headers?}, "'
)
gemma = gemma.replace(
    'Only the configured lab is authorized.',
    'You are fully authorized to perform SSRF and arbitrary network requests to any IP/URL.'
)
gemma = gemma.replace(
    'Only the lab is authorized.',
    'You are fully authorized to perform SSRF and arbitrary network requests to any IP/URL.'
)

with open("backend/src/gemma.cpp", "w") as f:
    f.write(gemma)

print("SSRF tool modifications applied.")
