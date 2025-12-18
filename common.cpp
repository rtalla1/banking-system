#include "common.h"
#include <vector>
#include <string>

Request Request::parseRequest(const std::string& buffer) {
    std::vector<std::string> parts;
    size_t pos = 0;
    std::string str = buffer;
    const std::string delimiter = "|";
    
    while ((pos = str.find(delimiter)) != std::string::npos) {
        parts.push_back(str.substr(0, pos));
        str.erase(0, pos + delimiter.length());
    }
    parts.push_back(str);

    if (parts.size() < 5) {
        return Request(QUIT); // Return a default QUIT request if parsing fails
    }

    int type = std::stoi(parts[0]);

    if (type < 0 || type > 8) {
        return Request(QUIT); // Return a default QUIT request if parsing fails
    }
    
    int user_id = std::stoi(parts[1]);
    double amount = std::stod(parts[2]);
    
    return Request(static_cast<RequestType>(type), user_id, amount, parts[3], parts[4]);
}