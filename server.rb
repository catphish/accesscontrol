require 'socket'
require 'securerandom'
require 'digest'
require 'thread'
require 'irb'
require 'openssl'

PACKET_TYPES = { 1 => 'Greeting', 6 => 'Card Accepted', 7 => 'Card Declined' }

Thread.abort_on_exception = true

$socket = UDPSocket.new
$socket.bind('10.210.1.11', 42424)

$psk = [127, 96, 69, 7, 254, 254, 216, 148, 191, 158, 1, 127, 10, 39, 30, 35, 180, 128, 12, 20, 63, 148, 101, 34, 35, 151, 228, 172, 225, 185, 235, 167].pack('C*')
$remote_ip        = nil
$remote_port      = nil
$remote_device_id = nil
$remote_counter   = nil
$remote_rng       = nil
$local_rng        = SecureRandom.random_bytes(32)
$local_counter    = 0
NULL_STRING_32    = "\0" * 32

def send_greeting
  packet = [$remote_device_id, 1, $local_counter, $local_rng, NULL_STRING_32].pack('Q>' 'C' 'N' 'a32' 'a32')
  $socket.send(packet, 0, $remote_ip, $remote_port)
end

def send_open
  $remote_counter += 1
  packet = [$remote_device_id, 2, $remote_counter, $remote_rng].pack('Q>' 'C' 'N' 'a32')
  packet << Digest::SHA256.digest($psk + Digest::SHA256.digest($psk+packet))
  $socket.send(packet, 0, $remote_ip, $remote_port)
end

def send_config
  $remote_counter += 1

  cards = [[0, 0, 0, 0, 8, 6, 0, 2, 7, 6, 7, 3, 6, 1]]

  data = cards.map{ |card| ([card.size] + card).pack('C*')}.join
  data = data.ljust(1024, "\0")
  iv = SecureRandom.random_bytes(16)
  cipher = OpenSSL::Cipher.new('AES-256-CBC')
  cipher.encrypt
  cipher.padding = 0
  cipher.key = $psk
  cipher.iv = iv
  encrypted_data = cipher.update(data) + cipher.final
  packet = [$remote_device_id, 5, $remote_counter, $remote_rng, NULL_STRING_32, iv, encrypted_data].pack('Q>' 'C' 'N' 'a32' 'a32' 'a16' 'a1024')
  packet << Digest::SHA256.digest($psk + Digest::SHA256.digest($psk+packet))
  $socket.send(packet, 0, $remote_ip, $remote_port)
end

Thread.new do
  loop do
    data, addr = $socket.recvfrom(1500)
                  device_id, packet_type, counter, rng,  header_hash, iv,   data,   data_hash =
      data.unpack('Q>'      'C'           'N'      'a32' 'a32'        'a16' 'a1024' 'a32')

    puts "Received packet"
    puts "  Device ID:   #{device_id}"
    puts "  Packet Type: #{PACKET_TYPES[packet_type]}"
    if iv && iv.bytesize == 16 && data && data.bytesize == 1024
      cipher = OpenSSL::Cipher.new('AES-256-CBC')
      cipher.decrypt
      cipher.key = $psk
      cipher.iv = iv
      cipher.padding = 0
      decrypted_data = cipher.update(data) + cipher.final
    else
      decrypted_data = nil
    end

    case packet_type
    when 1
      # Received greeting, send one back
      $remote_ip = addr[3]
      $remote_port = addr[1]
      $remote_device_id = device_id
      $remote_counter = counter
      $remote_rng = rng
      send_greeting
    when 6, 7
      length = decrypted_data.bytes[0]
      puts "  Card:      #{decrypted_data.bytes[1, length]}"
    end

  end
end

IRB.start
