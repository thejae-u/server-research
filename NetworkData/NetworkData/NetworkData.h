#pragma once
#ifdef _WIN32
#define NETWORK_DEFINES_API __declspec(dllexport)
#else
#define NETWORK_DEFINES_API
#endif

#include <string>
#include <cstddef>
#include <chrono>

extern "C" {

    // 열거형 정의
    enum class ENetworkType{
        // User Data Type
        LOGIN = 0,
        REGISTER,
        RETRIEVE,
        ACCESS,
        LOGOUT,

        MOVE = 100,
        ATTACK,
        DROP_ITEM,
        USE_ITEM,
        USE_SKILL,

		// System Data Type
        SYSTEM_OPTION_1 = 200,
		SYSTEM_OPTION_2,
		SYSTEM_OPTION_3,

        ADMIN_SERVER_OFF = 900,
        ADMIN_SERVER_ON,
		ADMIN_SERVER_REBOOT,
    };

    enum class ELoginError
    {
        USER_NOT_FOUND,
        USER_ALREADY_EXIST,
        INCORRECT,
    };

    // 구조체 정의
    struct NETWORK_DEFINES_API SNetworkData{
        std::string ip;
        std::string uuid;
        ENetworkType type;
        std::size_t bufSize;
        std::string data;
        std::chrono::system_clock::time_point createdAt;
    };
} // extern "C"

