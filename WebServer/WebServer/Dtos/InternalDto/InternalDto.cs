using System.ComponentModel.DataAnnotations;

namespace WebServer.Dtos.InternalDto;

public class GameSaveDto
{
    [Required]
    public required Guid GameId { get; set; }

    [Required]
    public required IEnumerable<UserSimpleDto> Users { get; set; }

    // 게임에 대한 내용을 포함 할 수 있음

    public DateTime StartedAt { get; set; }
    public DateTime FinishedAt { get; set; }
}