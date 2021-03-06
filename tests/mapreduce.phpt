--TEST--
Test mapreduce
--FILE--
<?php
include_once "connect.inc";

$alice1 = "Alice was beginning to get very tired of sitting by her sister on the ".
    "bank, and of having nothing to do: once or twice she had peeped into the ".
    "book her sister was reading, but it had no pictures or conversations in ".
    "it, 'and what is the use of a book,' thought Alice 'without pictures or ".
    "conversation?'";

$alice2 = "So she was considering in her own mind (as well as she could, for the ".
    "hot day made her feel very sleepy and stupid), whether the pleasure ".
    "of making a daisy-chain would be worth the trouble of getting up and ".
    "picking the daisies, when suddenly a White Rabbit with pink eyes ran ".
    "close by her.";

$alice3 = "The rabbit-hole went straight on like a tunnel for some way, and then ".
    "dipped suddenly down, so suddenly that Alice had not a moment to think ".
    "about stopping herself before she found herself falling down a very deep ".
    "well.";

try {
    $client = new RiakClient($host, $port);
    $alicebucket = new RiakBucket($client, "test_alice");

    $obj1 = new RiakObject("alice1");
    $obj1->data = $alice1;
    $alicebucket->putObject($obj1);

    $obj2 = new RiakObject("alice2");
    $obj2->data = $alice2;
    $alicebucket->putObject($obj2);

    $obj3 = new RiakObject("alice3");
    $obj3->data = $alice3;
    $alicebucket->putObject($obj3);

    $mrinput = new RiakMrInputKeyList(array("test_alice" => array("alice1", $obj2, $obj3)));

    $jsmapfunc = RiakMrJavascriptFunction::anon("function(v) {".
        "var m = v.values[0].data.toLowerCase().match(/\w*/g);".
        "var r = [];".
        "for(var i in m) {".
        "    if(m[i] != '') {".
        "        var o = {};".
        "        o[m[i]] = 1;".
        "        r.push(o);".
        "    }".
        "}".
        "return r;".
        "}");

    $jsredfunc = RiakMrJavascriptFunction::anon("function(v) {".
        "var r = {};".
        "for(var i in v) {".
        "   for(var w in v[i]) {".
        "       if(w in r) r[w] += v[i][w];".
        "       else r[w] = v[i][w];".
        "   }".
        "}".
        "return [r];".
        "}");

    $mr = new RiakMapreduce($client);
    $mr ->addPhase(new RiakMapreducePhase(RiakMapreducePhase::map, $jsmapfunc))
        ->addPhase(new RiakMapreducePhase(RiakMapreducePhase::reduce, $jsredfunc))
        ->setInput($mrinput);
    $json = $mr->toJson();
    $result = $mr->run();
    if ($result[0]->value[0]["the"] !== 8) {
        var_dump($result);
    }
    echo "success!";
} catch (Exception $e) {
  echo $e->getMessage();
}
?>
--EXPECT--
success!
