using AutoMapper;
using Microsoft.EntityFrameworkCore;
using Microsoft.OpenApi.Models;
using WebServer.Data;

namespace WebServer
{
    public class Program
    {
        public static void Main(string[] args)
        {
            var AllowSpecificOrigins = "_allowSpecificOrigins";
            var builder = WebApplication.CreateBuilder(args);

            // Add services to the container.

            builder.Services.AddControllers();
            builder.Services.AddEndpointsApiExplorer();
            builder.Services.AddSwaggerGen(options =>
            {
                options.SwaggerDoc("v1", new OpenApiInfo
                {
                    Title = "Game Server API",
                    Version = "v1",
                    Description = "Lockstep Web Server API Document",
                    Contact = new OpenApiContact
                    {
                        Name = "Kim Jae Woo",
                        Email = "contact@thejaeu.com"
                    }
                });
            });

            // DB Context
            var connectionString = builder.Configuration.GetConnectionString("DefaultConnection");
            builder.Services.AddDbContext<GameServerDbContext>(options =>
            {
                options.UseNpgsql(connectionString);
            });

            // docker container 보안 완화
            builder.Services.AddCors(options =>
            {
                options.AddPolicy(name: AllowSpecificOrigins,
                    policy =>
                    {
                        policy.WithOrigins("http://localhost", "null").AllowAnyHeader().AllowAnyMethod().AllowCredentials();
                    });
            });

            var app = builder.Build();

            // Configure the HTTP request pipeline.
            if (app.Environment.IsDevelopment())
            {
                app.UseSwagger();
                app.UseSwaggerUI(options =>
                {
                    options.SwaggerEndpoint("/swagger/v1/swagger.json", "Game Server API");
                    options.RoutePrefix = string.Empty;
                });
            }

            app.UseHttpsRedirection();

            app.UseAuthorization();

            app.MapControllers();

            app.Run();
        }
    }
}