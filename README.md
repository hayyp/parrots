# parrots: A Simple Proxy Server

Originally inspired by a MIT 6.824 Lab [assignment](https://pdos.csail.mit.edu/archive/6.824-2004/labs/webproxy1.html) and developed as an example for my [tpool](https://github.com/ahhzee/tpool.git) threadpool package, parrots is a simple web proxy that 

- can handle HTTP GET requests/responses of up to 65535 bytes (or responses will be truncated to fit in)

- uses `epoll` for I/O multiplexing (client-proxy only)

- uses `pthread` (tpool threadpool) for multithreading

# Build

To build parrots, you simply run `make`.

# System requirements

Since parrots uses `epoll`, it runs only on Linux systems.

# Todo

- Better error handling

- Support larger files (currently only 65535 bytes per request)

- Support HTTPS

- Support `kqueue`

- Support `Keep-Alive`

- Nonblocking connection with remote servers

# LICENSE

```plaintext

Copyright 2021 Zee

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

```
