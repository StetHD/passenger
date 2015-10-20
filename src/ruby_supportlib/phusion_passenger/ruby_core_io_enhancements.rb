# encoding: binary
#  Phusion Passenger - https://www.phusionpassenger.com/
#  Copyright (c) 2010-2014 Phusion Holding B.V.
#
#  "Passenger", "Phusion Passenger" and "Union Station" are registered
#  trademarks of Phusion Holding B.V.
#
#  See LICENSE file for license information.

PhusionPassenger.require_passenger_lib 'native_support'

class IO
  if defined?(PhusionPassenger::NativeSupport)
    # Writes all of the strings in the +components+ array into the given file
    # descriptor using the +writev()+ system call. Unlike IO#write, this method
    # does not require one to concatenate all those strings into a single buffer
    # in order to send the data in a single system call. Thus, #writev is a great
    # way to perform zero-copy I/O.
    #
    # Unlike the raw writev() system call, this method ensures that all given
    # data is written before returning, by performing multiple writev() calls
    # and whatever else is necessary.
    #
    #   io.writev(["hello ", "world", "\n"])
    def writev(components)
      return PhusionPassenger::NativeSupport.writev(fileno, components)
    end

    # Like #writev, but accepts two arrays. The data is written in the given order.
    #
    #   io.writev2(["hello ", "world", "\n"], ["another ", "message\n"])
    def writev2(components, components2)
      return PhusionPassenger::NativeSupport.writev2(fileno,
        components, components2)
    end

    # Like #writev, but accepts three arrays. The data is written in the given order.
    #
    #   io.writev3(["hello ", "world", "\n"],
    #     ["another ", "message\n"],
    #     ["yet ", "another ", "one", "\n"])
    def writev3(components, components2, components3)
      return PhusionPassenger::NativeSupport.writev3(fileno,
        components, components2, components3)
    end
  else
    def writev(components)
      return write(components.join(''))
    end

    def writev2(components, components2)
      data = ''
      components.each do |component|
        data << component
      end
      components2.each do |component|
        data << component
      end
      return write(data)
    end

    def writev3(components, components2, components3)
      data = ''
      components.each do |component|
        data << component
      end
      components2.each do |component|
        data << component
      end
      components3.each do |component|
        data << component
      end
      return write(data)
    end
  end

  if IO.method_defined?(:close_on_exec=)
    def close_on_exec!
      begin
        self.close_on_exec = true
      rescue NotImplementedError
      end
    end
  else
    require 'fcntl'

    if defined?(Fcntl::F_SETFD)
      def close_on_exec!
        fcntl(Fcntl::F_SETFD, Fcntl::FD_CLOEXEC)
      end
    else
      def close_on_exec!
      end
    end
  end
end