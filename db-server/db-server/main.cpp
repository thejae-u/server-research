#include <iostream>
#include <mysqlx/xdevapi.h>  // 최신 MySQL X DevAPI 헤더 파일
#include <chrono>
#include <iomanip>

int main() 
{
    try 
    {
        mysqlx::Session sess = mysqlx::getSession("localhost", 33060, "root", "thejaeu");
        mysqlx::Schema db = sess.getSchema("mmo_server_data");

		mysqlx::Table usersTable = db.getTable("users");

        int tmpCount = 0;

        for (auto row : usersTable.select("uuid", "user_name", "user_password", "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s')").execute())
        {
            std::cout << ++tmpCount << " uuid: " << row[0] << ", user_name: " << row[1] << "\npassword: " << row[2] << "\n";
			std::cout << "created_at: " << row[3] << "\n";

            std::cout << "\n";
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

