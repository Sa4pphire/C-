#include <iostream>
#include <optional>
#include <string>
#include <vector>

std::optional<std::string> find_name(
    const std::vector<std::string>& names,
    const std::string& target){
        for ( const std::string&name : names){
            {if (name == target)
                return name;
        }
    }
    return std::nullopt;
}

int main(){
    const std::vector<std::string> names{
        "Alice",
        "Bob",
        "Charlie",
    };
    const auto result = find_name(names,"Bob");

    if (result.has_value()){
        std::cout << "Find User:" << *result ;
    }
    else
    {
        std::cout << "没有找到用户";
    }
    return 0;   
}
