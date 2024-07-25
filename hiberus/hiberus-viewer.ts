import { IfcAPI, ms, IFCUNITASSIGNMENT } from '../dist/web-ifc-api';
import { IfcThree } from './web-ifc-three';
import { Init3DView, scene } from './web-ifc-scene';

let ifcAPI = new IfcAPI();
ifcAPI.SetWasmPath("")
let ifcThree = new IfcThree(ifcAPI);

if (typeof window != 'undefined')
{
    //@ts-ignore
    window.InitWebIfcViewer = async () => {
        await ifcAPI.Init();
        const fileInput = document.getElementById('finput');
        fileInput?.addEventListener('change', fileInputChanged);
        Init3DView();
    };
}

async function fileInputChanged() {
  let fileInput = <HTMLInputElement>document.getElementById('finput');
  if (fileInput?.files === null || fileInput.files.length == 0) return console.log('No files selected!');
  const file = fileInput.files[0];
  const reader = getFileReader(fileInput);
  reader.readAsArrayBuffer(file);
}

function getFileReader(fileInput: HTMLInputElement){
  var reader = new FileReader();
  reader.onload = () => {
    const data = getData(reader);
    LoadModel(data);
    fileInput.value = '';
  };
  return reader;
}

function getData(reader : FileReader){
  const startRead = ms();
  //@ts-ignore
  const data = new Uint8Array(reader.result);
  const readTime = ms() - startRead;
  console.log(`Reading took ${readTime} ms`);
  return data;
}

async function LoadModel(data: Uint8Array) {
    const start = ms();
    //TODO: This needs to be fixed in the future to rely on elalish/manifold
    const modelID = ifcAPI.OpenModel(data, { COORDINATE_TO_ORIGIN: true }); 
    const time = ms() - start;
    console.log(`Opening model took ${time} ms`);
    ifcThree.LoadAllGeometry(scene, modelID);


    if(ifcAPI.GetModelSchema(modelID) == 'IFC2X3' || 
    ifcAPI.GetModelSchema(modelID) == 'IFC4' ||
    ifcAPI.GetModelSchema(modelID) == 'IFC4X3_RC4')
    {   
        //Example to get all types used in the model
        let types = await ifcAPI.GetAllTypesOfModel(modelID);
        if(types)
        {
            for (let i = 0; i < types.length; i++) {
                let type = types[i];
                //console.log(type);
                //console.log(type.typeID);
                //console.log(type.typeName);
            }
        }
    }

    try
    {
        // This function should activate only if we are in IFC4X3
        let alignments = await ifcAPI.GetAllAlignments(modelID);
        console.log("Alignments: ", alignments);
    } catch (error) {
        // Code to handle the error
        console.error("An error occurred:", error);
    }

    let lines = ifcAPI.GetLineIDsWithType(modelID,  IFCUNITASSIGNMENT);
    //console.log(lines.size());
    for(let l = 0; l < lines.size(); l++)
    {
        //console.log(lines.get(l));
        let unitList = ifcAPI.GetLine(modelID, lines.get(l));
        //console.log(unitList);
        //console.log(unitList.Units);
        //console.log(unitList.Units.length);
        for(let u = 0; u < unitList.Units.length; u++) {
            //console.log(ifcAPI.GetLine(modelID, unitList.Units[u].value));
        }
    }
    ifcAPI.CloseModel(modelID);
}