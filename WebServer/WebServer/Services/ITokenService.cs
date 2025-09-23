using Microsoft.IdentityModel.Tokens;
using WebServer.Data;
using WebServer.Settings;

namespace WebServer.Services;

public interface ITokenService
{
    public string GenerateAccessToken(UserData user, double expireTime = 1);

    public string GenerateRefreshToken(UserData user, double expireTime = 30);

    public TokenValidationParameters GetTokenValidationParam();
}