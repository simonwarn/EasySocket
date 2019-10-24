# EasySocket
EasySocket is a winsock wrapper with basic functions like send chararrays and strings. Good for prototypeing or something to build upon.

Example of client
```cpp
int __cdecl main(int argc, char** argv)
{
	const char* buffer = "Hey i am sent from the client :D";

	while (true)
	{
		// Here we connect to our server by resolveing ip from a domain.
		EasySocket client("27015", "easysocket.tk" , true); // Port = 27015 , Address = easysocket.tk, isUrl = true.
                // Can also connect to an ip like this
                // EasySocket client("27015", "83.221.130.124"); // Port = 27015 , Address = 83.221.130.124, isUrl = false.
		if (client.IsOk())
		{
			if (client.Send(buffer)) // can also send a buffer client.Send(buffer , size)
			{
				std::cout << "Sent all" << std::endl;

				// Can also use std::vector<char> data = client.ReceiveBytes();
				std::string data = client.ReceiveStr();
				
				if (data.size())
					std::cout << "Got it back:" << std::string(data.data()) << std::endl;
				else
					std::cout << "Did not get anything back :(" << std::endl;
			}
			else
			{
				std::cout << "Send Failed!!!" << std::endl;
				break;
			}
			
			system("pause");
			system("cls");
		}
	}
}
```
Example of server
```cpp
int __cdecl main(int argc, char** argv)
{
	while (true)
	{
		EasySocket server("27015"); // Only providing port means its a server.

		if (server.IsOk())
		{
			if (server.ListenAndAccept())
			{
				std::vector<char> data = server.ReceiveBytes();

				if (data.size())
				{
					std::cout << "Got:" << std::string(data.data()) << std::endl;

					if (server.Send(data.data(), data.size()))
					{
						std::cout << "Sent it back" << std::endl;
					}
				}
				system("pause");
				system("cls");
			}
		}
	}
	return 0;
}
```
