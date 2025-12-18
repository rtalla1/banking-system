#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <chrono>

enum RequestType {
    QUIT,
    DEPOSIT,
    WITHDRAW,
    BALANCE,
    UPLOAD_FILE,
    DOWNLOAD_FILE,
    LOGIN,
    LOGOUT,
    EARN_INTEREST
};

struct Request {
    RequestType type;
    int user_id;
    double amount;
    std::string filename;
    std::string data;

    Request(RequestType t, int uid = 0, double amt = 0.0, 
            std::string fname = "", std::string d = "") : 
            type(t), user_id(uid), amount(amt), 
            filename(fname), data(d) {}

    static Request parseRequest(const std::string& buffer);
};

struct Response {
    bool success;
    double balance;
    std::string data;
    std::string message;

    Response(bool s = false, double b = 0.0, 
            std::string d = "", std::string m = "") :
            success(s), balance(b), data(d), message(m) {}
};

#endif