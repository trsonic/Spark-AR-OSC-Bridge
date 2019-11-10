// include modules
const Scene = require('Scene');
const Diagnostics = require('Diagnostics');
const Reactive = require('Reactive');
const DeviceMotion = require('DeviceMotion');
const NativeUI = require('NativeUI');
const Patches = require('Patches');
const WorldTransform = require('WorldTransform');
// get the position of the device in the world
const device = DeviceMotion.worldTransform;
//Diagnostics.watch('device.x', device.x);
//Diagnostics.watch('device.y', device.y);
//Diagnostics.watch('device.z', device.z);

var pixelPoint = Reactive.point2d(Reactive.val(0), Reactive.val(0))
// unproject a short distance. zero doesn't work
var camPos = Scene.unprojectWithDepth(pixelPoint, 0.01)

//var x = device.x;
//var y = device.y;
//var z = device.z;

var x = camPos.x;
var y = camPos.y;
var z = camPos.z;

// instantiate all the objects
var Bass = Scene.root.find('Bass');
var Drums = Scene.root.find('Drums');
var Guitar = Scene.root.find('Guitar');
var Vocals = Scene.root.find('Vocals');
var Keys = Scene.root.find('Keys');

//var planePosition = Reactive.point(Bass.transform.x.pinLastValue(), Bass.transform.y.pinLastValue(), Bass.transform.z.pinLastValue())
//Bass.transform.position = planePosition

//var objtext0 = Scene.root.find('objtext0');
//var objtext1 = Scene.root.find('objtext1');
//var objtext2 = Scene.root.find('objtext2');
//var objtext3 = Scene.root.find('objtext3');
//var objtext4 = Scene.root.find('objtext4');

// push into an array for easy iteration
var objs = [Bass, Drums, Guitar, Vocals, Keys];
// var objtexts = [objtext0, objtext1, objtext2, objtext3, objtext4];

Diagnostics.watch("rotx", device.rotationY);

// grab all the positions
for(var i = 0; i < objs.length; i++)
{
    //Diagnostics.log(objs[i].transform.x.pinLastValue());
    //Diagnostics.log(objs[i].worldTransform.x.pinLastValue());

    var objPos = Reactive.point(objs[i].worldTransform.x, objs[i].worldTransform.y, objs[i].worldTransform.z);
    //var r = Reactive.distance(objPos, device.position);
    var r = camPos.distance(objPos);
    var d = camPos.sub(objPos);
    var az = Reactive.atan2(d.x, d.z);
    az = Reactive.sub(az, device.rotationY);
    var PIE = Reactive.val(Math.PI);
    az = Reactive.mul(az, (Reactive.div(Reactive.val(-180), PIE)));
    //var el = Reactive.acos(Reactive.div(objs[i].worldTransform.y , r));
    
 
    

   var invr = Reactive.clamp(Reactive.sub(Reactive.val(1.0), r), Reactive.val(0.1), Reactive.val(1.0));



 //   invr = Reactive.sub(Reactive.val(1.0), Reactive.exp(invr));

    
  //  invr =  Reactive.clamp(Reactive.exp(invr), Reactive.val(0.1), Reactive.val(1.0)) ;

    //var clampr = Reactive.clamp(r, Reactive.val(0.1), Reactive.val(1.0));
    //var invclampr = Reactive.sub(Reactive.val(1.0), clampr); 

    //NativeUI.setText("objtext" + i, "hello world");
    //objtexts[i].text = invr.toString();

    //if(i == 0)
    //{
        //Diagnostics.watch(i + 'x', objs[i].worldTransform.x);
        //Diagnostics.watch(i + 'y', objs[i].worldTransform.y);
        //Diagnostics.watch(i + 'z', objs[i].worldTransform.z);
        //Diagnostics.watch(i + 'ra', r);
        //Diagnostics.watch(i + 'az', az);
        //Diagnostics.watch(i + 'el', el);
        //Diagnostics.watch(i + 'invr', invr);
    //}

    Patches.setScalarValue('r' + i, invr);
    Patches.setScalarValue('az'+ i, az);
}