using Microsoft.EntityFrameworkCore;
using WebServer.Data;
using WebServer.Dtos;

namespace WebServer.Services;

public class UserService : IUserService
{
    private readonly GameServerDbContext _gdbContext;
    private readonly ITokenService _tokenService;
    private readonly ICachingService _cachingService;

    public UserService(GameServerDbContext gdbContext, ITokenService tokenService, ICachingService cachingService)
    {
        _gdbContext = gdbContext;
        _tokenService = tokenService;
        _cachingService = cachingService;
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

        // TODO : 이미 온라인인 사용자에 대해서 로그인 요청이 들어오면 어떻게 처리 할 것인가?
        // 기존 로그인을 없애고 새로운 로그인으로 처리? 아니면 그냥 거부?
        if (await _cachingService.IsUserLoggedInAsync(user.UID))
        {
            return null; // 이미 온라인인 사용자
        }

        // User Caching in Redis
        if (!await _cachingService.SetUserLoginStatusAsync(user.UID))
        {
            throw new Exception("Internal error: user caching failed");
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

    public async Task<bool> LogoutAsync(UserSimpleDto userLogoutDto)
    {
        return await _cachingService.ClearUserLoginStatusAsync(userLogoutDto.UID);
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

        if (await _cachingService.IsUserLoggedInAsync(user.UID))
        {
            return null; // 이미 로그인 중인 사용자
        }

        await _cachingService.SetUserLoginStatusAsync(user.UID);
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