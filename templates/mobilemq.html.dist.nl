<!DOCTYPE html>
<!-- VERSION 2010-10-31 -->
<html>
<head>
  <title>Marquee</title>
  <meta http-equiv="Content-Type" content="text/html;charset=utf-8" >
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="Description" content="BallroomDJ Marquee">
  <meta http-equiv="Cache-Control" content="no-store">
  <style type="text/css">
    body {
      background-color: #33393b;
      margin: 0px;
      padding: 0px;
      font-size: 100%;
    }
    .mainbox {
      display: -webkit-flex;
      display: flex;
      flex-grow: 1;
      flex-shrink: 1;
      height: 95%;
      width: 95%;
      padding-left: 10pt;
      padding-top: 5pt;
    }
    .dispbox {
      display: -webkit-flex;
      display: flex;
      margin: auto;
      max-width: 450px;
      flex-grow: 1;
      flex-shrink: 1;
      flex-direction: column;
      justify-content: flex-start;
    }
    .linebox {
      margin-top: 5pt;
      display: -webkit-flex;
      display: flex;
      flex-direction: row;
      flex-wrap: nowrap;
      flex-grow: 1;
      flex-shrink: 2;
      justify-content: flex-start;
    }
    p {
      margin: 0;
      padding: 0;
    }
    .pbold {
      font-weight: bold;
    }
    .ptitle {
      font-style: italic;
      margin-left: 30px;
      color: #ffa600;
    }
    .pcol {
      color: #ffa600;
      font-size: 200%;
    }
    .plist {
      color: #ffffff;
      font-size: 200%;
    }
  </style>
</head>
<body>
  <div class="mainbox">
    <div class="dispbox">
      <div class="linebox">
        <p id="title" class="ptitle"></p>
      </div>
      <div class="linebox">
        <p id="current" class="pbold pcol"></p>
      </div>
      <div class="linebox">
        <p id="mq1" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq2" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq3" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq4" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq5" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq6" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq7" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq8" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq9" class="plist"></p>
      </div>
      <div class="linebox">
        <p id="mq10" class="plist"></p>
      </div>
    </div>
  </div>
  <script type="text/javascript">
var bdj = {};

bdj.updIntervalId = '';
bdj.chgInUse = false;
bdj.chgInUse = false;
bdj.speedIntervalId = '';
bdj.volumeIntervalId = '';
bdj.muteState = 0;

bdj.updateData = function (data) {
  var nm;

  var jd = '';
  if (data != '') {
    jd = JSON.parse (data);
  }

  var o = document.getElementById("title");
  if (! jd.title) {
    o.innerHTML = '';
  } else {
    o.innerHTML = jd.title;
  }
  o = document.getElementById("current");
  if (! jd.current) {
    o.innerHTML = "Not Playing";
  } else {
    o.innerHTML = jd.current;
  }

  if (jd.skip == 'true') {
    var mqlen = 10;
    var i;
    for (i = 1; i <= mqlen; ++i) {
      nm = "mq"+i;
      o = document.getElementById(nm);
      if (o) {
        o.innerHTML = "";
      }
    }
  } else {
    var mqlen = jd.mqlen;
    var i;
    for (i = 1; i <= mqlen; ++i) {
      nm = "mq"+i;
      o = document.getElementById(nm);
      if (o) {
        o.innerHTML = eval ("jd.mq"+i);
      }
    }
  }
}
bdj.doUpdate = function () {
  var tag = null;
//WEB  var urlParams = new URLSearchParams(window.location.search);
//WEB  tag = urlParams.get('tag');
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      bdj.updateData (xhr.responseText);
    }
  };
  if (tag) {
    xhr.open('GET', '/marquee4.php?v=2&tag='+tag, true);
  } else {
    xhr.open('GET', '/mmupdate', true);
  }
  xhr.send();
}
bdj.doLoad = function () {
  if (bdj.updIntervalId == '') {
    bdj.doUpdate();
    bdj.updIntervalId = setInterval(bdj.doUpdate,500);
  }
}

window.onload = function () { bdj.doLoad(); };
window.onresize = function () { bdj.doLoad(); };
  </script>
</body>
</html>
