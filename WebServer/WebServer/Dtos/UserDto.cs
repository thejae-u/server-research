using System.ComponentModel.DataAnnotations;

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
    public required string Token { get; set; } // JWT
    public required UserDto User { get; set; } // 유저 정보
}

public class UserDeleteDto
{
    [Required]
    public required Guid UID { get; set; }

    [Required]
    public required string Password { get; set; }

    [Required]
    public required bool Confirm { get; set; }
}

// 유저에게 노출되는 안전한 유저 데이터 (패스워드 미포함)
public class UserDto
{
    public Guid UID { get; set; }
    public required string Username { get; set; }
    public required string Role { get; set; }
    public DateTime CreatedAt { get; set; }
}