#pragma once
#ifdef _WIN32
#define NETWORK_DEFINES_API __declspec(dllexport)
#else
#define NETWORK_DEFINES_API
#endif

#include <string>
#include <cstddef>

extern "C" {

    // 열거형 정의
    enum class ENetworkType{
        // User Data Type
        LOGIN = 0,
        REGISTER,
        RETRIEVE,
        ACCESS,
        LOGOUT,

		// System Data Type
        SYSTEM_OPTION_1 = 100,
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
    };
} // extern "C"

