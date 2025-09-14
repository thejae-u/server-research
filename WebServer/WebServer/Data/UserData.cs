using System.ComponentModel.DataAnnotations;

namespace WebServer.Data;

public static class RoleCaching
{
    public static readonly string Player = "Player";
    public static readonly string Admin = "Admin";
    public static readonly string Internal = "Internal";
}

public class UserData
{
    [Key]
    public required Guid UID { get; set; }

    [Required]
    public required string Username { get; set; }

    [Required]
    public required string PasswordHash { get; set; }

    public DateTime CreatedAt { get; set; }

    public string Role { get; set; } = RoleCaching.Player;
}