using AutoMapper;
using WebServer.Data;
using WebServer.Dtos;

namespace WebServer.Profiles;

public class UserProfile : Profile
{
    public UserProfile()
    {
        CreateMap<UserData, UserDto>();
    }
}