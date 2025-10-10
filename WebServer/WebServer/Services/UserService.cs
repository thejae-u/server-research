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

    /// <summary>
    /// Internal Method for Get UserData or Check User Exist
    /// </summary>
    /// <param name="username">username string</param>
    /// <returns>User Data or Null</returns>
    private async Task<UserData?> GetUserByUserName(string username)
    {
        if (string.IsNullOrEmpty(username)) return null;
        return await _gdbContext.Users.FirstOrDefaultAsync(u => u.Username == username);
    }

    public async Task<UserDto?> RegisterAsync(UserRegisterDto userRegisterDto, bool isAdmin = false, bool isInternal = false)
    {
        if (await GetUserByUserName(userRegisterDto.Username) is not null)
            return null; // Already Exist

        if (userRegisterDto.Password.Length < 8)
            return null; // password must be over 8 words

        var role = RoleCaching.Player;

        if (isAdmin)
        {
            role = RoleCaching.Admin;
        }
        else if (isInternal)
        {
            role = RoleCaching.Internal;
        }

        var user = new UserData
        {
            Uid = Guid.NewGuid(),
            Username = userRegisterDto.Username,
            PasswordHash = BCrypt.Net.BCrypt.HashPassword(userRegisterDto.Password),
            Role = role,
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
        var user = await GetUserByUserName(userLoginDto.Username);

        if (user == null || !BCrypt.Net.BCrypt.Verify(userLoginDto.Password, user.PasswordHash))
        {
            return null; // 사용자가 없거나 비밀번호가 틀림
        }

        // User Caching in Redis
        if (!await _cachingService.SetUserLoginStatusAsync(user.Uid))
        {
            throw new Exception("Internal error: user caching failed");
        }

        // TODO : 이미 로그인 중인 사용자에 대한 처리 필요
        // 로그인 사용자와 로그인을 사용자 모두 Token에 대한 정보를 삭제 -> 재 접속 하도록 유도

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

    public async Task<InternalResponseDto?> LoginInternalAsync(UserLoginDto userLoginDto)
    {
        var internalUser = await GetUserByUserName(userLoginDto.Username);
        if (internalUser is null) return null;

        if ((internalUser.Role.Equals(RoleCaching.Admin) || internalUser.Role.Equals(RoleCaching.Internal)) == false) // if not internal user
            return null;

        // Internal은 RefreshToken을 발급하지 않음
        var accessTokenString = _tokenService.GenerateAccessToken(internalUser);
        return new InternalResponseDto
        {
            AccessToken = accessTokenString,
        };
    }

    public async Task FlushInternalUserAsync()
    {
        var internalList = await _gdbContext.Users.Where(u => u.Role == RoleCaching.Admin || u.Role == RoleCaching.Internal).ToListAsync();
        foreach (var user in internalList)
        {
            _gdbContext.Remove(user);
        }
    }

    public async Task<bool> LogoutAsync(UserSimpleDto userLogoutDto)
    {
        if (await GetUserByUserName(userLogoutDto.Username) is null) return false;
        return await _cachingService.ClearUserLoginStatusAsync(userLogoutDto.Uid);
    }

    public async Task<UserResponseDto?> RefreshAsync(string refreshToken)
    {
        var responseFromTokenService = await _tokenService.ValidateRefreshToken(refreshToken);
        if (responseFromTokenService is null ||
            responseFromTokenService.Uid is null ||
            responseFromTokenService.Principal is null) return null;

        var userId = responseFromTokenService.Uid;
        var user = await _gdbContext.Users.FindAsync(userId);

        if (user is null) return null;

        // 새로 로그인 처리
        await _cachingService.SetUserLoginStatusAsync(user.Uid);
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
        var user = await _gdbContext.Users.FirstOrDefaultAsync(u => u.Uid == userDeleteDto.Uid);
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