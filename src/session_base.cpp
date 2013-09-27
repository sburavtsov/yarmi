#include <yarmi/session_base.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

namespace yarmi {

/***************************************************************************/

struct session_base::impl {
	impl(boost::asio::io_service &ios, global_context_base &gcb)
		:socket(ios)
		,gcb(gcb)
	{}

	boost::asio::ip::tcp::socket socket;
	global_context_base &gcb;
}; // struct session_base::impl

/***************************************************************************/

session_base::session_base(boost::asio::io_service &ios, global_context_base &gcb)
	:pimpl(new impl(ios, gcb))
{}

session_base::~session_base()
{ delete pimpl; }

/***************************************************************************/

boost::asio::ip::tcp::socket &session_base::get_socket()
{ return pimpl->socket; }

/***************************************************************************/

void session_base::start() {
	auto self = this->shared_from_this();

	enum { header_size = sizeof(yas::uint32_t)+YARMI_IARCHIVE_TYPE::_header_size };
	std::shared_ptr<char> header_buffer(new char[header_size], [](char *ptr){delete []ptr;});

	auto read_body = [this, self, header_buffer](const boost::system::error_code &ec, std::size_t rd) {
		if ( ec || rd != header_size ) {
			std::cerr << "header read error: " << ec.message() << std::endl;
			return;
		}

		YARMI_IARCHIVE_TYPE ia(header_buffer.get(), header_size);
		std::uint32_t body_length = 0;
		ia & body_length;

		std::shared_ptr<char> body_buffer(new char[body_length], [](char *ptr){delete []ptr;});
		auto body_readed = [this, self, body_buffer, body_length](const boost::system::error_code &ec, std::size_t rd) {
			if ( ec || rd != body_length ) {
				std::cerr << "body read error: " << ec.message() << std::endl;
				return;
			}

			try {
				on_received(body_buffer.get(), body_length);
			} catch (const std::exception &ex) {
				std::cerr << "exception is thrown when invoking: " << ex.what() << std::endl;
			}

			start();
		};

		boost::asio::async_read(
			 pimpl->socket
			,boost::asio::buffer(body_buffer.get(), body_length)
			,body_readed
		);
	};

	boost::asio::async_read(
		 pimpl->socket
		,boost::asio::buffer(header_buffer.get(), header_size)
		,read_body
	);

}

/***************************************************************************/

void session_base::stop() { pimpl->socket.cancel(); }
void session_base::close() { pimpl->socket.close(); }

/***************************************************************************/

void session_base::send(const yas::shared_buffer &buffer) {
	auto self = this->shared_from_this();

	boost::asio::async_write(
		 pimpl->socket
		,boost::asio::buffer(buffer.data.get(), buffer.size)
		,[this, self, buffer](const boost::system::error_code &ec, std::size_t) {
			if ( ec ) throw std::runtime_error("session_base::send() error: "+ec.message());
		}
	);
}

/***************************************************************************/

void session_base::on_yarmi_error(yas::uint8_t call_id, yas::uint8_t version_id, const std::string &msg) {
	std::cerr << "on_yarmi_error(" << (int)call_id << ", " << (int)version_id << "): '" << msg << "'" << std::endl << std::flush;
}

/***************************************************************************/

} // ns yarmi
