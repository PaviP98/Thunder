#include "DoorBell.h"

#ifdef __WIN32__
#include <Winsock2.h>
#include <ws2tcpip.h>
#endif

namespace WPEFramework {

namespace Core {

    DoorBell::Connector::Connector(DoorBell& parent, const Core::NodeId& node)
        : _parent(parent)
        , _doorbell(node)
        , _socket(::socket(_doorbell.Type(), SOCK_DGRAM, 0))
        , _bound(0)
    {
    }
    /* virtual */ DoorBell::Connector::~Connector()
    {
        if ((_bound & 0x01) == 1) {
            ResourceMonitor::Instance().Unregister(*this);
        }
#ifdef __WIN32__
        ::closesocket(_socket);
#else
        ::close(_socket);
#endif
    }

    bool DoorBell::Connector::Bind() const
    {
        if (_bound == 0) {

#ifndef __WIN32__
            // Check if domain path already exists, if so remove.
            if (_doorbell.Type() == NodeId::TYPE_DOMAIN) {
                if (access(_doorbell.HostName().c_str(), F_OK) != -1) {
                    TRACE_L1("Found out domain path already exists, deleting: %s", _doorbell.HostName().c_str());
                    remove(_doorbell.HostName().c_str());
                }
            }
#endif
            if ((_doorbell.Type() == NodeId::TYPE_IPV4) || (_doorbell.Type() == NodeId::TYPE_IPV6)) {
                // set SO_REUSEADDR on a socket to true (1): allows other sockets to bind() to this
                // port, unless there is an active listening socket bound to the port already. This
                // enables you to get around those "Address already in use" error messages when you
                // try to restart your server after a crash.
                int optval = 1;
                socklen_t optionLength = sizeof(int);

                ::setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, optionLength);
            }

#ifdef __WIN32__
            unsigned long l_Value = 1;
            if (ioctlsocket(_socket, FIONBIO, &l_Value) != 0) {
                TRACE_L1("Error on port socket NON_BLOCKING call. Error %d", ::WSAGetLastError());
            } else {
                _bound = 1;
            }
#else
            if (fcntl(_socket, F_SETOWN, getpid()) == -1) {
                TRACE_L1("Setting Process ID failed. <%d>", errno);
            } else {
                int flags = fcntl(_socket, F_GETFL, 0) | O_NONBLOCK;

                if (fcntl(_socket, F_SETFL, flags) != 0) {
                    TRACE_L1("Error on port socket F_SETFL call. Error %d", errno);
                } else {
                    _bound = 1;
                }
            }
#endif

            if ((_bound == 1) && (::bind(_socket, static_cast<const NodeId&>(_doorbell), _doorbell.Size()) != SOCKET_ERROR)) {

#ifndef __WIN32__
                if ((_doorbell.Type() == NodeId::TYPE_DOMAIN) && (_doorbell.Rights() <= 0777)) {
                    _bound = ((::chmod(_doorbell.HostName().c_str(), _doorbell.Rights()) == 0) ? 1 : 0);
                }
#endif

                if (_bound == 1) {
                    _bound = (SocketPort::enumState::UPDATE | 1);
                    ResourceMonitor::Instance().Register(*const_cast<Connector*>(this));
                }
            } else {
                _bound = 0;
            }
        } else if (_bound == SocketPort::enumState::UPDATE) {
            _bound |= 1;
            ResourceMonitor::Instance().Register(*const_cast<Connector*>(this));
        }

        return (_bound);
    }

    void DoorBell::Connector::Unbind() const
    {
        if ((_bound & 0x01) == 1) {
            ResourceMonitor::Instance().Unregister(*const_cast<Connector*>(this));
            _bound = SocketPort::enumState::UPDATE;
        }
	}

    /* virtual */ IResource::handle DoorBell::Connector::Descriptor() const
    {
        return (static_cast<IResource::handle>(_socket));
    }

    /* virtual */ uint16_t DoorBell::Connector::Events()
    {
#ifdef __WIN32__
        uint16_t result = ((_bound & SocketPort::enumState::UPDATE) | FD_READ);
        _bound &= ~SocketPort::enumState::UPDATE;
        return (result);
#else
        return (POLLIN);
#endif
    }

    /* virtual */ void DoorBell::Connector::Handle(const uint16_t events)
    {
#ifdef __WIN32__
        if ((events & FD_READ) != 0) {
            Read();
        }
#else
        if ((events & POLLIN) != 0) {
            Read();
        }
#endif
    }

    DoorBell::DoorBell(const TCHAR sourceName[])
        : _connectPoint(*this, Core::NodeId(sourceName))
        , _signal(false, true)
    {
    }

    DoorBell::~DoorBell()
    {
    }
}
}