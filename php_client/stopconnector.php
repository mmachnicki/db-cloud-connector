<?php
$param = "";

if($argc>1){
    $param = $argv[1];
}

define("EOL", "\r\n");
define("EOF", "\0");

$server = "localhost";

stopServer($server, $param);    

function stopServer($url, $param){
    $server = gethostbyname($url);
    $port = 8080;    
    $socket = @socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
            
    if($socket === false){
        write("socket_create() failed due to: ".socket_strerror(socket_last_error()));        
        return NULL;
    }
    write("Attempting to connect $server on port $port");
    $result = @socket_connect($socket, $server, $port);
        
    if($result === false){
        write("Socket_connect() failed due to: ".socket_strerror(socket_last_error($socket)));
        socket_close($socket);
        return NULL;
    }else{
        write("Socket connected");
    }    
    
    $message = "Sending STOP message.";    
    
    $sent = sendMessage($socket, $message);
    if($sent ===  false){
        write("socket_send() failed due to: ".socket_strerror(socket_last_error($socket))); 
        socket_close($socket);
        return false;
    }else{
        write("Server stopped.");
    }    
    socket_close($socket);
    
    return true;
}
  
function write($message){
    echo $message."\r\n";
//    Log::write($message);
}

function sendMessage($socket, $message){
    return @socket_send($socket, makeSocketFrame($message), strlen($message)+128, NULL);
}

function makeSocketFrame($string) {
    $stringSize = strlen($string);
    $bytes = 4;
    $dSize = sprintf("%0" . ($bytes * 8) . "b", $stringSize);
    $sdSize = "";

    for ($i = 0; $i < $bytes; $i++) {
        $s = substr($dSize, $i * 8, 8);
        $dc = bindec($s);
        if ($dc == 0) {
            $sdSize .= "\0";
        } else {
            $sdSize .= chr($dc);
        }
    }
    $ids = "";
    for($i = 0; $i<60; $i++){
        $ids .= "\0";
    }        
    
    $meta = "stop";
    for($i = 0; $i<60; $i++){
        $meta .= "\0";
    }        
    $frame = $sdSize.$ids.$meta.$string."\0";
    
    
    return $frame;
}
