#pragma once

#include <string>

// HTTP 상태 코드의 카테고리를 정의하는 열거형 클래스
enum class HttpStatusCodeCategory {
	Success,
	Redirection,
	ClientError,
	ServerError,
	Unknown
};

HttpStatusCodeCategory GetStatusCodeCategory(long statusCode);
std::string ToString(HttpStatusCodeCategory category);
