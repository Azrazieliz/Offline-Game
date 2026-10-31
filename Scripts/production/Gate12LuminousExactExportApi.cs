using System;
using System.Collections;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using BepInEx;
using KKAPI.Maker;
using UnityEngine;
using Debug = UnityEngine.Debug;

// Dedicated API-driven Koikatsu Maker/KKBP bridge for confirmed Alter cards.
// No UI clicks, no foreground-window calls, no source-card mutations.
[BepInPlugin("noworld.gate12.correct_three_clothed.api.batch", "NoWorld Gate12 Original Three Luminous Card Export API", "1.0.0")]
public sealed class Gate12LuminousExactExportApi : BaseUnityPlugin
{
    static readonly string Root = @"D:\UnrealProjects\G12_LuminousNativeImport_20261031\Saved\CorrectKoikatsuExports";
    static readonly string GameExports = @"D:\Tools\Steam\steamapps\common\Koikatsu Party\Export_PMX";
    static readonly string Inputs = @"D:\UnrealProjects\Gate12_WholeCharacter_Build_20261031\Assets\NoWorld\Luminous\KoikatsuOriginalCards";
    static readonly string[] States = {"AWAKENED_CLOTHED","BERSERKER_CLOTHED","GRAND_CLOTHED"};
    static readonly string[] Cards = {
       Path.Combine(Inputs,@"LUMINOUS_AWAKENED_CLOTHED_KK.png"),
       Path.Combine(Inputs,@"LUMINOUS_BERSERKER_CLOTHED_KK.png"),
       Path.Combine(Inputs,@"LUMINOUS_GRAND_CONVERGENCE_CLOTHED_KK.png")
    };
    static readonly string[] Hashes = {
       "e6b53e82c0d87ecdbd0ff8bf510ce93a1d7e557b436382271fcfed6451897bd5",
       "820307ce971382af9b620fa8610af60918ad01cc7267994ba9ac5cde96070862",
       "26c48e1c03c154f9f39a0b06afaade53423c11dbd079604e430162eb4cae5353"
    };
    bool entered = false, working = false, completed = false;
    float nextPoll;
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    static extern bool ShowWindow(IntPtr handle, int command);

    static void Note(string text)
    {
        Directory.CreateDirectory(Root);
        File.AppendAllText(Path.Combine(Root, "Gate12_OriginalThreeCard_API_Export.log"),
            DateTime.UtcNow.ToString("o")+" "+text+Environment.NewLine);
    }
    static string Hash(string file)
    {
        using (var fs=File.OpenRead(file))
        using (var sha=SHA256.Create())
           return BitConverter.ToString(sha.ComputeHash(fs)).Replace("-","").ToLowerInvariant();
    }
    void Awake(){Note("API_PLUGIN_LOADED no_GUI_clicks");}
    void Update()
    {
        if (completed || working || Time.realtimeSinceStartup<nextPoll) return;
        nextPoll=Time.realtimeSinceStartup+1.0f;
        // This plugin runs only for this explicit, single-use batch.
        if(!File.Exists(Path.Combine(Root,"Gate12_OriginalThreeCard_API_BATCH_AUTHORIZED.flag"))) return;
        try {
            // Conceal only our own game window, without taking focus.
            var handle=Process.GetCurrentProcess().MainWindowHandle;
            if (handle!=IntPtr.Zero) ShowWindow(handle,0);
            if (MakerAPI.InsideAndLoaded && MakerAPI.GetCharacterControl()!=null){
                working=true;
                StartCoroutine(ExportBoth());
                return;
            }
            if (!entered && Time.realtimeSinceStartup>2.0f) {
                var title=UnityEngine.Object.FindObjectOfType<TitleScene>();
                if (title!=null) {
                    entered=true;
                    Note("API_ENTER_FEMALE_MAKER");
                    title.OnCustomFemale();
                }
            }
        }catch(Exception error){Note("API_INIT_ERROR "+error);completed=true;}
    }
    IEnumerator ExportBoth()
    {
        for(int i=0;i<States.Length;i++)
        {
            var state=States[i];
            {
                if (!File.Exists(Cards[i])||Hash(Cards[i])!=Hashes[i])
                    throw new IOException("Frozen card source/hash mismatch");
                var cha=MakerAPI.GetCharacterControl();
                if (cha==null)throw new InvalidOperationException("Maker ChaControl unavailable");
                Note("LOAD_APPROVED_SOURCE state="+state+" sha256="+Hashes[i]);
                bool loaded=cha.chaFile.LoadFileLimited(Cards[i],cha.sex,true,true,true,true,true);
                int nativeError=cha.chaFile.GetLastErrorCode();
                Note("CHA_NATIVE_LOADER_RETURN="+loaded+" error_code="+nativeError+" (the KKBP exporter itself checks the error code, not the bool)");
                if(nativeError>0)
                    throw new IOException("Native ChaFileControl error code="+nativeError);
                cha.ChangeCoordinateType(true);
                bool reloaded=cha.Reload(false,false,false,false);
                if(!reloaded)throw new InvalidOperationException("Native ChaControl.Reload failed");
                yield return new WaitForSeconds(2.0f);
                var kkbp=AppDomain.CurrentDomain.GetAssemblies()
                   .FirstOrDefault(a=>a.GetName().Name=="KKBP_Exporter");
                if (kkbp==null)throw new InvalidOperationException("Installed KKBP exporter assembly not loaded");
                var typ=kkbp.GetType("PmxExpoter",true);
                // KKBP declares a static Instance field but does NOT populate it in Awake.
                // Resolve the actual BepInEx MonoBehaviour through Unity native object lookup.
                var instance=UnityEngine.Object.FindObjectOfType(typ) as MonoBehaviour;
                if(instance==null)throw new InvalidOperationException("Native KKBP exporter MonoBehaviour missing");
                var cfg=typ.GetField("exportConfig",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(instance);
                var cfgType=cfg.GetType();
                foreach(var name in new[]{"batchExport","exportAllOutfits","exportAllVariations","exportHitBoxes","exportCurrentPose","exportWithEnabledShapekeys"}) {
                    var fi=cfgType.GetField(name,BindingFlags.Instance|BindingFlags.Public|BindingFlags.NonPublic);
                    if(fi!=null)fi.SetValue(cfg,false);
                }
                var builder=Activator.CreateInstance(kkbp.GetType("PmxBuilder",true),true);
                typ.GetField("builder",BindingFlags.Instance|BindingFlags.NonPublic).SetValue(instance,builder);
                var before=Directory.GetDirectories(GameExports);
                Note("KKBP_NATIVE_START_BUILD state="+state);
                var build=(IEnumerator)typ.GetMethod("StartBuild",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(instance,null);
                yield return instance.StartCoroutine(build);
                var added=Directory.GetDirectories(GameExports).Except(before).ToArray();
                var chosen=added.SelectMany(d=>Directory.GetFiles(d,"model.pmx",SearchOption.TopDirectoryOnly))
                    .Where(p=>new FileInfo(p).Length>10000).ToArray();
                if(chosen.Length!=1)throw new IOException("Expected one fresh original PMX; actual="+chosen.Length);
                var pmx=chosen[0];
                Note("EXPORTED_CONFIRMED state="+state+" pmx="+pmx+" pmx_bytes="+new FileInfo(pmx).Length+" pmx_sha256="+Hash(pmx));
                File.WriteAllText(Path.Combine(Root,"Gate12_Card_"+state+"_Original_API_Export.json"),
                   "{\"status\":\"EXPORTED_NATIVE_EXACT_ORIGINAL_CARD\",\"state\":\""+state+
                   "\",\"source_card_sha256\":\""+Hashes[i]+
                   "\",\"pmx_path\":\""+pmx.Replace("\\","\\\\")+
                   "\",\"pmx_sha256\":\""+Hash(pmx)+"\"}");
            }
            yield return null;
        }
        completed=true;
        Note("ALL_THREE_EXACT_ORIGINAL_CLOTHED_CARDS_NATIVE_KKBP_EXPORTED");
    }
}
