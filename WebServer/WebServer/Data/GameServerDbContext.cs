using Microsoft.EntityFrameworkCore;

namespace WebServer.Data;

public class GameServerDbContext : DbContext
{
    public GameServerDbContext(DbContextOptions options) : base(options)
    {
    }

    public DbSet<UserData> Users { get; set; }
}