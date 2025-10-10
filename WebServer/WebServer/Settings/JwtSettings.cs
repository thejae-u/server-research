using System.ComponentModel.DataAnnotations;

namespace WebServer.Settings;

public class JwtSettings
{
    public required string Issuer { get; set; } = string.Empty;
    public required string Audience { get; set; } = string.Empty;

    [Required]
    public required string AccessKey { get; set; } = string.Empty;

    [Required]
    public required string RefreshKey { get; set; } = string.Empty;

    public static bool Validate(JwtSettings? settings)
    {
        return settings != null
            && !string.IsNullOrEmpty(settings.AccessKey)
            && !string.IsNullOrEmpty(settings.Issuer)
            && !string.IsNullOrEmpty(settings.Audience)
            && !string.IsNullOrEmpty(settings.RefreshKey);
    }
}