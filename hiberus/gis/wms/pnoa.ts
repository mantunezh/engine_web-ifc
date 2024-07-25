import * as L from 'leaflet'

// center latitude and longitude
const get_generic_map = ({url, layers, center, width, height}:{url: string, layers: string,center:number[], width: number, height: number}) =>{
	const map = L.map(
	    'wms',
	    {    
	        center: [center[0], center[1]],
	        zoom: 19
			}
	)
	map.setMaxBounds(
		L.latLngBounds(
			[center[0] - width / 2, center[1] - height / 2],
			[center[0] + width / 2, center[1] + height / 2]
		)
	)
		
	L.tileLayer.wms(
		url, 
		{
	    layers: layers,
	    format: 'image/jpeg',
	    transparent: true
		}
	).addTo(map)

	return map
}

const get_pnoa_ma = ({center, width, height}:{center:number[], width: number, height: number}) =>{
	return get_generic_map(
		{
			url: 'https://www.ign.es/wms-inspire/pnoa-ma',
			layers: 'OI.OrthoimageCoverage',
			center,
			width,
			height
		})
}

export const PNOA = {
	get_generic_map,
	get_pnoa_ma
}