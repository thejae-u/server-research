using System.ComponentModel.DataAnnotations;

namespace WebServer.Data;

public class GameData
{
    [Required]
    public required Guid GameId { get; set; }

    [Required]
    public required IEnumerable<Guid> Users { get; set; }
}