using System;
using System.Collections.Generic;
using UnityEngine.Networking;

namespace Utility
{
    public enum EHttpMethod
    {
        GET,
        POST,
        PUT,
        DELETE
    }

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

        public const string API_MATCHMAKING_START = "/api/matchmaking/start";
        public const string API_MATCHMAKING_CHECKSTATUS = "/api/matchmaking/checkstatus";
    }

    [Serializable]
    public class RefreshRequest
    {
        public string refreshToken;
    }

    [Serializable]
    public class UserDto
    {
        public string Uid;
        public string Username;
        public string Role;
        public string CreatedAt;
    }

    [Serializable]
    public class UserSimpleDto
    {
        public Guid Uid { get; set; }

        public string Username { get; set; }
    }

    [Serializable]
    public class LoginResponse
    {
        public string AccessToken;
        public string RefreshToken;
        public UserDto User;
    }

    [Serializable]
    public class InternalGroupDto
    {
        public Guid GroupId { get; set; }
        public string Name { get; set; }
        public UserSimpleDto Owner { get; set; }
        public List<UserSimpleDto> Players { get; set; }
        public DateTime CreatedAt { get; set; }
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

    [Serializable]
    public class GroupStatusResponseDto
    {
        public bool status { get; set; }

        public string serverIp { get; set; }
        public ushort port { get; set; }
    }
}