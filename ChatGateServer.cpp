#include <json/json.h>
#include <iostream>

int main() {
    Json::Value root;
    root["name"] = "ChatGPT";
    root["year"] = 2025;

    Json::StreamWriterBuilder writer;
    std::string output = Json::writeString(writer, root);

    std::cout << output << std::endl;
    return 0;
}
