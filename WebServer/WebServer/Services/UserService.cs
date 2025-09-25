using Microsoft.EntityFrameworkCore;
using System.Security.Claims;
using WebServer.Data;
using WebServer.Dtos;

namespace WebServer.Services;

public class UserService : IUserService
{
    private readonly GameServerDbContext _gdbContext;
    private readonly ITokenService _tokenService;

    public UserService(GameServerDbContext gdbContext, ITokenService tokenService)
    {
        _gdbContext = gdbContext;
        _tokenService = tokenService;
    }

    public async Task<UserDto?> RegisterAsync(UserRegisterDto userRegisterDto)
    {
        if (await _gdbContext.Users.AnyAsync(u => u.Username == userRegisterDto.Username))
            return null; // Already Exsist

        if (userRegisterDto.Password.Length < 8)
            return null; // password must be over 8 words

        var user = new UserData
        {
            UID = Guid.NewGuid(),
            Username = userRegisterDto.Username,
            PasswordHash = BCrypt.Net.BCrypt.HashPassword(userRegisterDto.Password),
            Role = RoleCaching.Player,
            CreatedAt = DateTime.UtcNow
        };

        // DB 반영
        await _gdbContext.AddAsync(user);
        await _gdbContext.SaveChangesAsync();

        // UserDto로 직접 매핑하여 반환
        return new UserDto().Mapping(user);
    }

    public async Task<UserResponseDto?> LoginAsync(UserLoginDto userLoginDto)
    {
        var user = await _gdbContext.Users.FirstOrDefaultAsync(u => u.Username == userLoginDto.Username);

        if (user == null || !BCrypt.Net.BCrypt.Verify(userLoginDto.Password, user.PasswordHash))
        {
            return null; // 사용자가 없거나 비밀번호가 틀림
        }

        // Token Generate
        var accessTokenString = _tokenService.GenerateAccessToken(user); // Default 1 Hour expire time
        var refreshTokenString = await _tokenService.GenerateRefreshToken(user);

        // Convert UserData to UserDto
        var userDto = new UserDto().Mapping(user);
        return new UserResponseDto
        {
            AccessToken = accessTokenString,
            RefreshToken = refreshTokenString,
            User = userDto
        };
    }

    public async Task<UserResponseDto?> RefreshAsync(string refreshToken)
    {
        var responseFromTokenService = await _tokenService.ValidateRefreshToken(refreshToken);
        if (responseFromTokenService is null ||
            responseFromTokenService.UserId is null ||
            responseFromTokenService.Principal is null) return null;

        var userId = responseFromTokenService.UserId;
        var principal = responseFromTokenService.Principal;
        var user = await _gdbContext.Users.FindAsync(userId);

        if (user is null) return null;

        var newAccessToken = _tokenService.GenerateAccessToken(user);

        return new UserResponseDto
        {
            AccessToken = newAccessToken,
            RefreshToken = refreshToken,
            User = new UserDto().Mapping(user)
        };
    }

    public async Task<UserDto?> GetUSerByIdAsync(Guid id)
    {
        var user = await _gdbContext.Users.FindAsync(id);
        var userDto = user == null ? null : new UserDto().Mapping(user);
        return userDto;
    }

    public async Task<bool> DeleteUserAsync(UserDeleteDto userDeleteDto)
    {
        var user = await _gdbContext.Users.FirstOrDefaultAsync(u => u.UID == userDeleteDto.UID);
        if (user == null || !BCrypt.Net.BCrypt.Verify(userDeleteDto.Password, user.PasswordHash))
            return false;

        _gdbContext.Users.Remove(user);
        await _gdbContext.SaveChangesAsync();

        return true;
    }

    public async Task<IEnumerable<UserDto>> GetAllUserAsync()
    {
        var users = await _gdbContext.Users.ToListAsync();

        var userDtoList = new List<UserDto>();
        foreach (var user in users)
        {
            userDtoList.Add(new UserDto().Mapping(user));
        }

        return userDtoList;
    }
}