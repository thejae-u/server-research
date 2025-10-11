#include "HttpStatus.h"

HttpStatusCodeCategory GetStatusCodeCategory(long statusCode) {
	if (statusCode >= 200 && statusCode < 300) {
		return HttpStatusCodeCategory::Success;
	}
	if (statusCode >= 300 && statusCode < 400) {
		return HttpStatusCodeCategory::Redirection;
	}
	if (statusCode >= 400 && statusCode < 500) {
		return HttpStatusCodeCategory::ClientError;
	}
	if (statusCode >= 500 && statusCode < 600) {
		return HttpStatusCodeCategory::ServerError;
	}
	return HttpStatusCodeCategory::Unknown;
}

std::string ToString(HttpStatusCodeCategory category) {
	switch (category) {
	case HttpStatusCodeCategory::Success:
		return "Success";
	case HttpStatusCodeCategory::Redirection:
		return "Redirection";
	case HttpStatusCodeCategory::ClientError:
		return "Client Error";
	case HttpStatusCodeCategory::ServerError:
		return "Server Error";
	case HttpStatusCodeCategory::Unknown:
	default:
		return "Unknown";
	}
}