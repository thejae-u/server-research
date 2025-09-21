using Microsoft.IdentityModel.Tokens;
using System.Text;
using WebServer.Settings;

namespace WebServer.Utils;

public static class TokenUtils
{
    public static JwtSettings GetJwtSettings(IConfiguration config)
    {
        var jwtSettings = config.GetSection("Jwt").Get<JwtSettings>();
        if (jwtSettings == null
            || string.IsNullOrEmpty(jwtSettings.Issuer)
            || string.IsNullOrEmpty(jwtSettings.Audience)
            || string.IsNullOrEmpty(jwtSettings.Key))
        {
            throw new ArgumentNullException(nameof(jwtSettings));
        }

        return jwtSettings;
    }

    public static TokenValidationParameters GetTokenValidationParam(JwtSettings jwtSettings)
    {
        var validateionParmeters = new TokenValidationParameters
        {
            ValidateIssuer = true,
            ValidIssuer = jwtSettings.Issuer,

            ValidateAudience = true,
            ValidAudience = jwtSettings.Audience,

            ValidateLifetime = true,
            ValidateIssuerSigningKey = true,
            IssuerSigningKey = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtSettings.Key)),

            ClockSkew = TimeSpan.Zero
        };

        return validateionParmeters;
    }
}

public class TokenValidateRequest
{
    public required string Token { get; set; }
}