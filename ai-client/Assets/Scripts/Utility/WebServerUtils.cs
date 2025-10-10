using System;
using System.Collections.Generic;

namespace Utility
{
    public static class WebServerUtils
    {
        public const string API_SERVER_IP = "http://localhost:18080";
        public const string API_AUTH_REGISTER = "/api/auth/register";
        public const string API_AUTH_LOGIN = "/api/auth/login";
        public const string API_AUTH_REFRESH = "/api/auth/refresh";
        public const string API_GROUP_JOIN = "/api/group/join";
        public const string API_GROUP_LEAVE = "/api/group/leave";
        public const string API_GROUP_CREATE = "/api/group/create";
        public const string API_GROUP_GET_ALL = "/api/group/info/lobby";
        public const string API_GROUP_GET_INFO = "/api/group/info";
    }

    [Serializable]
    public class RefreshRequest
    {
        public string refreshToken;
    }

    [Serializable]
    public class UserDto
    {
        public string uid;
        public string username;
        public string role;
        public string createdAt;
    }

    [Serializable]
    public class UserSimpleDto
    {
        public Guid uid { get; set; }

        public string username { get; set; }
    }

    [Serializable]
    public class LoginResponse
    {
        public string accessToken;
        public string refreshToken;
        public UserDto user;
    }

    [Serializable]
    public class GroupDto
    {
        public Guid groupId { get; set; }

        public string name { get; set; }

        public UserSimpleDto owner { get; set; }

        public List<UserSimpleDto> players { get; set; }

        public DateTime createdAt { get; set; }
    }

    [Serializable]
    public class CreateGroupRequestDto
    {
        public string groupName;
        public UserSimpleDto requester;
    }

    [Serializable]
    public class JoinGroupRequestDto
    {
        public Guid groupId { get; set; }

        public UserSimpleDto requester { get; set; }
    }
}