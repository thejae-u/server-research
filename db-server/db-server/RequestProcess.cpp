#include "RequestProcess.h"
#include "db-system-utility.h"

RequestProcess::RequestProcess(std::shared_ptr<mysqlx::Schema> db) : _dbPtr(db)
{
}

RequestProcess::~RequestProcess()
{
}

ELastErrorCode RequestProcess::RetreiveUserID(std::vector<std::string> userName)
{
	assert(userName.size() != 0);

	// select Users
	try
	{
		auto data = GetTable(USER_TABLE).select("uuid").where("user_name = :name").bind("name", userName[0]).execute();

		if (data.count() == 0)
		{
			return ELastErrorCode::USER_NOT_FOUND;
		}
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	return ELastErrorCode::USER_ALREADY_EXIST;
}

ELastErrorCode RequestProcess::Login(std::vector<std::string> loginData)
{
	assert(loginData.size() == 2);

	try
	{
		auto data = GetTable(USER_TABLE)
			.select("uuid")
			.where("user_name = :name AND user_password = SHA2(:pass, 256)")
			.bind("name", loginData[0])
			.bind("pass", loginData[1])
			.execute();

		if (data.count() == 0)
		{
			return ELastErrorCode::INCORRECT;
		}
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::Register(std::vector<std::string> registerData)
{
	assert(registerData.size() == 2);

	try
	{
		GetTable(USER_TABLE)
			.insert("user_name", "user_password")
			.values(registerData[0], registerData[1])
			.execute();

	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::SaveServerLog(std::string log)
{
	try
	{
		GetTable("server_log").insert("log_text").values(log).execute(); // insert log to server_log table
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
	}

	std::cout << System_Util::GetCurrentTime() << " " << log << "\n";

	return ELastErrorCode::SUCCESS;
}
