using Microsoft.IdentityModel.Tokens;
using WebServer.Data;
using WebServer.Settings;

namespace WebServer.Services;

public interface ITokenService
{
    public string GenerateToken(UserData user, double expireTime = 1);

    public TokenValidationParameters GetTokenValidationParam();
}