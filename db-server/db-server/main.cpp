#include <iostream>
#include <mysqlx/xdevapi.h>  // 최신 MySQL X DevAPI 헤더 파일
#include <chrono>
#include <iomanip>

void printTimestamp(const mysqlx::Value& value) {
    if (value.isNumber()) {
        std::time_t time = static_cast<std::time_t>(value); // TIMESTAMP 값 변환
        std::tm* tm = std::gmtime(&time); // UTC 기준으로 변환

        std::cout << "Timestamp: " << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << std::endl;
    }
    else {
        std::cout << "Invalid timestamp format." << std::endl;
    }
}

int main() 
{
    try 
    {
        mysqlx::Session sess = mysqlx::getSession("localhost", 33060, "root", "thejaeu");
        mysqlx::Schema db = sess.getSchema("mmo_server_data");

		mysqlx::Table usersTable = db.getTable("users");

        int tmpCount = 0;

        for (auto row : usersTable.select("uuid", "user_name", "user_password", "created_at").execute())
        {
            std::cout << ++tmpCount << " uuid: " << row[0] << ", user_name: " << row[1] << "\npassword: " << row[2] << "\n";
            
            auto created_at = row[3].

        }
    }
    catch (const mysqlx::Error& err) {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
    catch (std::exception& ex) {
        std::cerr << "STD Exception: " << ex.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown error!" << std::endl;
        return 1;
    }

    return 0;
}

