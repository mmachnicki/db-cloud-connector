<?php
$param = "";

if($argc>1){
    $param = $argv[1];
}

define("EOL", "\r\n");
define("EOF", "\0");

$server = "localhost";

while(true){
    $socket = getSocket($server, $param);
    if($socket != NULL) run($socket, $param);
    sleep(5);
}

function run($socket, $param){
    write("Attempting to receive socket data.");
        
    $iter = 0;
    while(true){
        $mLength = "";
        $buff = "";
        $noMessage = true;
        
        
        if(sendMessage($socket, $param) === false) {
            write("socket_send() failed due to: ".socket_strerror(socket_last_error($socket))); 
            break;
        }else{
//            write("Sent:$param");
        }
                
        
        if(@socket_recv($socket, $mLength, 4, MSG_WAITALL) > 0){
            $readout = @socket_recv($socket, $buff, 459745, MSG_DONTWAIT);
            
            $requestID = substr($buff, 0, 30);            
            $responseID = substr($buff, 30, 30);            
            $metacommand = trim(substr($buff, 60, 64));
            $message = substr($buff, 124);
                
            if($metacommand == "masterdb_off"){
                write("$iter::RECEIVED @time:".time()."::MASTER DB IS OFF, BACKING OFF");
                sleep(3);
            }
            else write("$iter::RECEIVED @time:".time()."::".  strlen($message)." BYTES: ".$message);                
            $iter++;
//            usleep(500);
            sleep(3);
        }                
        else {
            usleep (10000);
        }
    }    
    socket_close($socket);
    write("socket disconnected");
}

function getSocket($url, $param){
    $server = gethostbyname($url);
    $port = 8080;    
    $socket = @socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    socket_set_option($socket, SOL_SOCKET, SO_RCVTIMEO, array("sec"=>3, "usec"=>0));
            
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
    
    
    $message = "I pretend to be a cron job of Magento, hi.";
    
    
    $sent = sendMessage($socket, $message);
    if($sent ===  false){
        write("socket_send() failed due to: ".socket_strerror(socket_last_error($socket))); 
        socket_close($socket);
        return NULL;
    }else{
        write("Initial message sent");
    }    
    return $socket;
}

function generateKey() {
    $chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!"$&/()=[]{}0123456789';
    $key = '';
    $chars_length = strlen($chars);
    for ($i = 0; $i < 16; $i++) $key .= $chars[mt_rand(0, $chars_length-1)];
    return base64_encode($key);
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
    $meta = "";
    for($i = 0; $i<124; $i++){
        $meta .= "\0";
    }    
    
    write("SENDING@time:".time()."::".$stringSize." BYTES; ".$string);
    $frame = $sdSize.$meta.$string."\0";
    
    
    return $frame;
}
