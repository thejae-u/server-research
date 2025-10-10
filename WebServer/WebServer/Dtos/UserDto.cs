using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;
using System.Security.Claims;
using WebServer.Data;

namespace WebServer.Dtos;

// 유저 회원가입 데이터
public class UserRegisterDto
{
    [Required]
    public required string Username { get; set; }

    [Required]
    [MinLength(8)]
    public required string Password { get; set; }
}

// 유저 로그인 데이터
public class UserLoginDto
{
    [Required]
    public required string Username { get; set; }

    [Required]
    public required string Password { get; set; }
}

// 로그인 성공 시 반환 데이터
public class UserResponseDto
{
    public required string AccessToken { get; set; }
    public required string RefreshToken { get; set; }
    public required UserDto User { get; set; } // 유저 정보
}

public class InternalResponseDto
{
    public required string AccessToken { get; set; }
}

public class UserDeleteDto
{
    [Required]
    public required Guid Uid { get; set; }

    [Required]
    public required string Password { get; set; }

    [Required]
    public required bool Confirm { get; set; }
}

// 유저에게 노출되는 안전한 유저 데이터 (패스워드 미포함)
public class UserDto
{
    public Guid Uid { get; set; }
    public required string Username { get; set; }
    public required string Role { get; set; }
    public DateTime CreatedAt { get; set; }

    [SetsRequiredMembers]
    public UserDto()
    {
        Username = "None";
        Role = "Player";
    }

    [SetsRequiredMembers]
    public UserDto(string role)
    {
        Username = "None";
        Role = role;
    }

    public UserDto Mapping(UserData user)
    {
        Uid = user.Uid;
        Username = user.Username;
        Role = user.Role;
        CreatedAt = user.CreatedAt;
        return this;
    }
}

public class UserSimpleDto
{
    [Required]
    public required Guid Uid { get; set; }

    public required string Username { get; set; }
}

public class UserDtoForTokenResponse
{
    public Guid? Uid { get; set; }
    public ClaimsPrincipal? Principal { get; set; }
}