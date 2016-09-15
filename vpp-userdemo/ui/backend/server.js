// =========================================================================
// ==================== get the packages we need ===========================
// =========================================================================
var express       = require('express');
var app           = express();
var v1            = new express.Router(); 
var bodyParser    = require('body-parser');
var ip            = require('ip');
var fs            = require('fs');
var path          = require('path');
var fs            = require('fs');
var untildify     = require('untildify');
var Client        = require('ssh2').Client;
var conn          = new Client();
var PATH          = untildify('~/');
var winston       = require('winston');
var exec          = require('child_process').exec;
// ===========================================================================
// ================== Initialize the logging system ==========================
// ===========================================================================
var logger = new (winston.Logger)({
  transports: [
    new (winston.transports.File)({
      name: 'info-file',
      filename: 'node-info.log',
      level: 'info'
    }),
    new (winston.transports.File)({
      name: 'error-file',
      filename: 'node-error.log',
      level: 'error'
    })
  ]
});
// ==========================================================================
// ====================== start the webserver  ==============================
// ==========================================================================
var server = app.listen(5000, ip.address(), function() {
  console.log('Server running at http://'+ip.address()+':'+5000);
  logger.log('info', 'Server running at http://'+ip.address()+':'+5000);
});
// ===========================================================================
// ================ Initialize express dependencies ==========================
// ===========================================================================
app.use(bodyParser.urlencoded({extended: false}));
app.use(bodyParser.json());
app.use(express.static(__dirname + '/../dist.prod'));
// Removing Angular's annoying # from the URL
app.get('/', function(req, res) {
  res.sendFile(path.join(__dirname + '/../dist.prod/html/index.html'));
});
// ===========================================================================
// ================= Initialize the API routes ===============================
// ===========================================================================
app.use('/api/v1', v1);

var TUTORIALS_PATH  = __dirname+'/../../tutorials';
var CMDS_REGEXP     = /CMD\+=\("(.*)"\)/gmi;
var INSTR_REGEXP    = /INSTR\+=\("(.*)"\)/gmi;
var C1_IP_REGEXP    = /C1_IP="(.*)"/gmi;
var C1_GW_REGEXP    = /C1_GW="(.*)"/gmi;
var C2_IP_REGEXP    = /C2_IP="(.*)"/gmi;
var C2_GW_REGEXP    = /C2_GW="(.*)"/gmi;


function grabTutorials(req, res, next){
  
  var TUTORIALS     = [];
  fs.readdir(TUTORIALS_PATH, function(err, data) {
    if (err) {
     return console.log(err);
   }
else
   for(var x = 0; x < data.length; x++){
      if(data[x].match(/^[^.]+$|.*\.sh/g)){
        TUTORIALS.push(data[x]);
      }
    }
    logger.log('info', 'Tutorials are sent to the client');
    res.send(TUTORIALS);
  });
}

function grabFile(req, res, next){

  fs.readFile(TUTORIALS_PATH+'/'+req.params.tutorial, 'utf8', function (err,data) {
    if (err) {
      return console.log(err);
    }
    var CMD          = '';
    var commands     = [];
    var INSTR        = '';
    var instructions = [];
    var IP1          = '';
    var IP2          = '';
    var GW1          = '';
    var GW2          = '';

    CMD   = data.match(CMDS_REGEXP);
    INSTR = data.match(INSTR_REGEXP);
    IP1   = data.match(C1_IP_REGEXP);
    IP2   = data.match(C2_IP_REGEXP);
    GW1   = data.match(C1_GW_REGEXP);
    GW2   = data.match(C2_GW_REGEXP);

    IP1   = IP1[0].replace('C1_IP="', "").replace('"', "");
    IP2   = IP2[0].replace('C2_IP="', "").replace('"', "");
    GW1   = GW1[0].replace('C1_GW="', "").replace('"', "");
    GW2   = GW2[0].replace('C2_GW="', "").replace('"', "");

    for(var x = 0; x < INSTR.length; x++){
      commands.push(CMD[x].replace('CMD+=("', "").replace('")', ""));
      instructions.push(INSTR[x].replace('INSTR+=("', "").replace('")', ""));
    }
    exec('/vagrant/netns.sh '+IP1+' '+GW1+' '+IP2+' '+GW2, function(error, stdout, stderr) {
      res.send({commands: commands, instructions: instructions});
    });
  });
}

function runCommand(req, res, next){
  //Removing the out of bounds error
  if(req.body.command == ''){req.body.command = ' '}
  exec(req.body.command, function(error, stdout, stderr) {
    if(stderr){res.send(stderr)}
    else if(stdout){res.send(stdout)}
    else{res.send(error)}
  });
}

v1.get('/tutorials', grabTutorials);
v1.get('/file/:tutorial', grabFile);
v1.post('/command', runCommand);
