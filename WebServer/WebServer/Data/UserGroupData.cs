using System.ComponentModel.DataAnnotations;
using System.Collections.Generic;

namespace WebServer.Data;

public class UserGroupData
{
    [Key]
    [Required]
    public required Guid GroupId { get; set; } // 그룹 고유 ID (게임 서버로부터 가져옴)

    [Required]
    public required List<Guid> Players { get; set; } // 그룹에 속한 플레이어 목록

    [Required]
    public required DateTime CreatedAt { get; set; } // 그룹 생성 시점
}