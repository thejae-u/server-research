using Microsoft.IdentityModel.Tokens;
using System.Security.Claims;
using WebServer.Data;
using WebServer.Dtos;

namespace WebServer.Services;

public interface ITokenService
{
    public string GenerateAccessToken(UserData user, double expireTime = 1);

    public Task<string> GenerateRefreshToken(UserData user, double expireTime = 30);

    public Task<UserDtoForTokenResponse?> ValidateRefreshToken(string refreshToken);

    public TokenValidationParameters GetTokenValidationParam();
}