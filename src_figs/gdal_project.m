function varargout = gdal_project(varargin)
% Helper Window to do raster projections with GDAL

%	Copyright (c) 2004-2011 by J. Luis
%
% 	This program is part of Mirone and is free software; you can redistribute
% 	it and/or modify it under the terms of the GNU Lesser General Public
% 	License as published by the Free Software Foundation; either
% 	version 2.1 of the License, or any later version.
% 
% 	This program is distributed in the hope that it will be useful,
% 	but WITHOUT ANY WARRANTY; without even the implied warranty of
% 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
% 	Lesser General Public License for more details.
%
%	Contact info: w3.ualg.pt/~jluis/mirone
% --------------------------------------------------------------------

	if isempty(varargin)
		errordlg('GDAL_PROJECT: wrong number of input arguments.','Error'),	return
	end
 
	hObject = figure('Tag','figure1','Visible','off');
	gdal_project_LayoutFcn(hObject);
	handles = guihandles(hObject);

	handMir = varargin{1};
	if (handMir.no_file)
		errordlg('Toc Toc! There is nothing hereee. What are you expecting?','ERROR')
		delete(hObject),	return
	end
	move2side(handMir.figure1, hObject,'right')
	handles.handMir = handMir;
	handles.hMirFig = handMir.figure1;
	handles.hdr.ULx = handMir.head(1) - handMir.head(8) / 2 * (~handMir.head(7));	% Goto pixel reg
	handles.hdr.ULy = handMir.head(4) + handMir.head(9) / 2 * (~handMir.head(7));
	handles.hdr.Xinc = handMir.head(8);    handles.hdr.Yinc = handMir.head(9);

	handles.nRows = '';		handles.nCols = '';
	handles.xInc = '';		handles.yInc = '';

	% Set the default projections ofered here
	handles.projGDAL_name = {''; 'Geog'; 'Mercator'; 'Tansverse Mercator'; 'UTM';'Miller'; ...
			'Lambert Equal Area'; 'Gall (Stereographic)'; 'Equidistant Cylindrical'; 'Cassini'; ...
			'Sinusoidal'; 'Mollweide'; 'Robinson'; 'Eckert IV'; 'Eckert VI'; 'Goode Homolosine'; ...
			'Lambert Conformal Conic'; 'Equidistant Conic'; ...
			'Albers Equal Area'; 'Lambert Equal Area'; 'Polyconic'; ...
			'Bonne'; 'Polar Stereographic'; 'Gnomonic'; 'Ortographic'; 'Van der Grinten'};
	
	handles.projGDAL_pars = {''; '+proj=latlong'; '+proj=merc'; '+proj=tmerc +lat_0=0 +lon_0=-9'; '+proj=utm +zone=29 +datum=WGS84'; ...
			'+proj=mill'; '+proj=cea'; ...
			'+proj=gall'; '+proj=eqc'; '+proj=cass +lon_0=0'; '+proj=sinu'; '+proj=moll +lon_0=0'; ...
			'+proj=robin +lon_0=0'; '+proj=eck4'; '+proj=eck6'; '+proj=goode'; '+proj=lcc +lat_1=20n +lat_2=60n'; ...
			'+proj=eqdc +lat_1=15n +lat_2=75n'; ...
			'+proj=aea +lat_1=20n +lat_2=60n'; '+proj=laea +lat_1=20n +lat_2=60n'; '+proj=poly'; ...
			'+proj=bonne'; '+proj=stere +lat_ts=71 +lat_0=90 +lon_0=0'; '+proj=gnom'; '+proj=ortho'; '+proj=vandg';};

	set(handles.popup_projections,'String',handles.projGDAL_name)

	% See if we have something to put in the source edit box
 	handles.have_prjIn = false;			% It will be true when input is projected and we know how
	proj4 = getappdata(handles.hMirFig,'Proj4');
	if (~isempty(proj4))
		set(handles.edit_source,'String',proj4)
		handles.have_prjIn = true;
	else
		projWKT = getappdata(handles.hMirFig,'ProjWKT');
		if (~isempty(projWKT))
			proj4 = ogrproj(projWKT);
			set(handles.edit_source,'String',proj4)
			handles.have_prjIn = true;
		elseif (handMir.geog)
			set(handles.edit_source,'String','+proj=latlong')
		end
	end

	%------------ Give a Pro look (3D) to the frame box ----------------------------
	new_frame3D(hObject, handles.text_IM, handles.frame1)

	% Add this figure handle to the carra?as list
	plugedWin = getappdata(handles.hMirFig,'dependentFigs');
	plugedWin = [plugedWin hObject];
	setappdata(handles.hMirFig,'dependentFigs',plugedWin);

	guidata(hObject, handles);
	set(hObject,'Visible','on');
	if (nargout), 	varargout{1} = hObject;		end

% -----------------------------------------------------------------------------------------
function edit_nRows_CB(hObject, handles)
	xx = str2double(get(hObject,'String'));
	if (isnan(xx)),		set(hObject,'String',handles.nRows),	return,	end
	handles.nRows = abs(round(xx));
	guidata(handles.figure1, handles)

% -----------------------------------------------------------------------------------------
function edit_nCols_CB(hObject, handles)
	xx = str2double(get(hObject,'String'));
	if (isnan(xx)),		set(hObject,'String',handles.nCols),	return,	end
	handles.nCols = abs(round(xx));
	guidata(handles.figure1, handles)

% -----------------------------------------------------------------------------------------
function edit_xInc_CB(hObject, handles)
	xx = str2double(get(hObject,'String'));
	if (isnan(xx)),		set(hObject,'String',handles.xInc),	return,	end
	handles.xInc = abs(xx);
	guidata(handles.figure1, handles)

% -----------------------------------------------------------------------------------------
function edit_yInc_CB(hObject, handles)
	xx = str2double(get(hObject,'String'));
	if (isnan(xx)),		set(hObject,'String',handles.yInc),	return,	end
	handles.yInc = abs(xx);
	guidata(handles.figure1, handles)

% -----------------------------------------------------------------------------------------
function push_OK_CB(hObject, handles)

	cmap = [];
	str_src = deblank(get(handles.edit_source,'String'));
	str_dst = deblank(get(handles.edit_target,'String'));
	
	if ( isempty(str_dst) ),	return,		end
	
	handles.hdr.DstProjSRS = str_dst;
	if ( ~isempty(str_src) && ~strcmp(str_src,'+proj=latlong') )
		handles.hdr.SrcProjSRS = str_src;
	end
	handles.hdr.t_size = 0;

	% Get interpolation method
	if (get(handles.radio_bilinear,'Val'))
		handles.hdr.ResampleAlg = 'bilinear';
	elseif (get(handles.radio_cubic,'Val'))
		handles.hdr.ResampleAlg = 'cubic';
	elseif (get(handles.radio_cubicspline,'Val'))
		handles.hdr.ResampleAlg = 'spline';
	else
		% nearest - but it's already the default
	end

	% See if we size or resolution requests
	if (~isempty(handles.xInc))			% Resolution takes precedence
		handles.hdr.t_res = [handles.xInc handles.xInc];
		if (~isempty(handles.yInc))
			handles.hdr.t_res(2) = handles.yInc;
		end
	elseif ( ~isempty(handles.nCols) && ~isempty(handles.nRows) )
		handles.hdr.t_size = [handles.nCols handles.nRows];
	end

	if (handles.handMir.validGrid)
		[X,Y,Z] = load_grd(handles.handMir);
		tipo = 'grid';
	else
		Z = get(handles.handMir.hImg,'CData');
		tipo = 'image';
		if (ndims(Z) == 2),		cmap = get(handles.handMir.figure1,'Colormap');		end
	end
	[ras, att] = gdalwarp_mex(Z, handles.hdr);
	
	if (numel(ras) < 4)
		errordlg('Sorry but the operation went wrong. We got nothing valuable on return.','Error'),	return
	end

	if (handles.handMir.validGrid)
		tmp.X = linspace(att.GMT_hdr(1),att.GMT_hdr(2), size(ras,2));
		tmp.Y = linspace(att.GMT_hdr(3),att.GMT_hdr(4), size(ras,1));
	else
		tmp.X = att.GMT_hdr(1:2);		tmp.Y = att.GMT_hdr(3:4);
	end
	tmp.head = att.GMT_hdr;
	prjName = handles.projGDAL_name{get(handles.popup_projections,'Value')};
	tmp.name = ['Reprojected (' prjName ') ' tipo];
	tmp.srsWKT = att.ProjectionRef;
	if (~isempty(cmap)),	tmp.cmap = cmap;	end

	mirone(ras,tmp)

% -----------------------------------------------------------------------------------------
function popup_projections_CB(hObject, handles)
	prj = handles.projGDAL_pars{get(hObject,'Value')};
	set(handles.edit_target,'String',prj)

% -----------------------------------------------------------------------------------------
function radio_near_CB(hObject, handles)
	if (~get(hObject,'Value')),		set(hObject,'Value',1),		return,		end
	set([handles.radio_bilinear handles.radio_cubic handles.radio_cubicspline],'Val',0)

% -----------------------------------------------------------------------------------------
function radio_bilinear_CB(hObject, handles)
	if (~get(hObject,'Value')),		set(hObject,'Value',1),		return,		end
	set([handles.radio_near handles.radio_cubic handles.radio_cubicspline],'Val',0)

% -----------------------------------------------------------------------------------------
function radio_cubic_CB(hObject, handles)
	if (~get(hObject,'Value')),		set(hObject,'Value',1),		return,		end
	set([handles.radio_near handles.radio_bilinear handles.radio_cubicspline],'Val',0)

% -----------------------------------------------------------------------------------------
function radio_cubicspline_CB(hObject, handles)
	if (~get(hObject,'Value')),		set(hObject,'Value',1),		return,		end
	set([handles.radio_near handles.radio_bilinear handles.radio_cubic],'Val',0)


% --- Creates and returns a handle to the GUI figure. 
function gdal_project_LayoutFcn(h1)

set(h1,...
'Color',get(0,'factoryUicontrolBackgroundColor'),...
'MenuBar','none',...
'Name','GDAL project',...
'NumberTitle','off',...
'PaperSize',[20.98404194812 29.67743169791],...
'Position',[520 619 540 241],...
'Resize','off',...
'HandleVisibility','callback',...
'Tag','figure1');

uicontrol('Parent',h1,...
'FontName','Helvetica',...
'Position',[17 30 35 15],...
'String','Rows',...
'Style','text');

uicontrol('Parent',h1, 'Position',[10 55 331 35], 'Style','frame', 'Tag','frame1');

uicontrol('Parent',h1,...
'FontName','Helvetica',...
'Position',[10 138 155 18],...
'String','Destination Referecing System',...
'Style','text');

uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'HorizontalAlignment','left',...
'Max',3,...
'Position',[10 168 521 41],...
'Style','edit',...
'Tooltip','This is the Proj4 definition string that describes the source coordinate system. Blank, defaults to geogs',...
'Tag','edit_source');

uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'HorizontalAlignment','left',...
'Max',3,...
'Position',[10 99 521 41],...
'Style','edit',...
'Tooltip','Write here a Proj4 definition string with the target coordinate sysyem. Blank, defaults to geogs',...
'Tag','edit_target');

uicontrol('Parent',h1,...
'FontName','Helvetica',...
'Position',[15 210 140 16],...
'String','Source Referecing System',...
'Style','text');

uicontrol('Parent',h1,...
'Call',{@gdal_project_uiCB,h1,'push_OK_CB'},...
'FontName','Helvetica',...
'FontSize',9,...
'Position',[465 10 60 21],...
'String','OK',...
'Tag','push_OK');

uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Call',{@gdal_project_uiCB,h1,'popup_projections_CB'},...
'Position',[380 214 151 22],...
'String',{'Popup Menu'},...
'Style','popupmenu',...
'Value',1,...
'Tag','popup_projections');

uicontrol('Parent',h1, 'Position',[316 218 61 15],...
'FontName','Helvetica',...
'String','Projections',...
'Style','text');

uicontrol('Parent',h1,...
'Call',{@gdal_project_uiCB,h1,'radio_near_CB'},...
'FontName','Helvetica',...
'Position',[20 64 125 16],...
'String','nearest neighbour',...
'Style','radiobutton',...
'TooltipString','nearest neighbour resampling (fastest algorithm, worst interpolation quality)',...
'Tag','radio_near');

uicontrol('Parent',h1,...
'Call',{@gdal_project_uiCB,h1,'radio_bilinear_CB'},...
'FontName','Helvetica',...
'Position',[138 64 70 16],...
'String','bilinear',...
'Style','radiobutton',...
'TooltipString','bilinear resampling',...
'Value',1,...
'Tag','radio_bilinear');

uicontrol('Parent',h1,...
'Call',{@gdal_project_uiCB,h1,'radio_cubic_CB'},...
'FontName','Helvetica',...
'Position',[207 64 60 16],...
'String','cubic',...
'Style','radiobutton',...
'TooltipString','cubic resampling',...
'Tag','radio_cubic');

uicontrol('Parent',h1,...
'Call',{@gdal_project_uiCB,h1,'radio_cubicspline_CB'},...
'FontName','Helvetica',...
'Position',[264 64 85 16],...
'String','cubicspline',...
'Style','radiobutton',...
'TooltipString','cubic spline resampling',...
'Tag','radio_cubicspline');

uicontrol('Parent',h1,...
'FontName','Helvetica',...
'Position',[30 81 110 16],...
'String','Interpolation method',...
'Style','text',...
'Tag','text_IM');

uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Call',{@gdal_project_uiCB,h1,'edit_nRows_CB'},...
'Position',[10 10 49 21],...
'Style','edit',...
'TooltipString','Set up number of rows of output. Leave blank for automatic guess',...
'Tag','edit_nRows');

uicontrol('Parent',h1, 'Position',[60 10 49 21],...
'BackgroundColor',[1 1 1],...
'Call',{@gdal_project_uiCB,h1,'edit_nCols_CB'},...
'Style','edit',...
'TooltipString','Set up number of columns of output. Leave blank for automatic guess',...
'Tag','edit_nCols');

uicontrol('Parent',h1, 'Position',[109 7 89 30],...
'FontName','Helvetica',...
'String',{'OR Resolution'; '(blank -> auto)'},...
'Style','text');

uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Call',{@gdal_project_uiCB,h1,'edit_xInc_CB'},...
'Position',[197 10 71 21],...
'Style','edit',...
'TooltipString','Set up X resolution of output. Leave blank for automatic guess',...
'Tag','edit_xInc');

uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Call',{@gdal_project_uiCB,h1,'edit_yInc_CB'},...
'Position',[269 10 71 21],...
'Style','edit',...
'TooltipString','Set up Y resolution of output. Leave blank for automatic guess',...
'Tag','edit_yInc');

uicontrol('Parent',h1, 'Position',[62 31 45 15],...
'FontName','Helvetica',...
'String','Columns',...
'Style','text');

uicontrol('Parent',h1,...
'FontName','Helvetica',...
'Position',[217 31 35 15],...
'String','x inc',...
'Style','text');

uicontrol('Parent',h1,...
'FontName','Helvetica',...
'Position',[289 31 35 15],...
'String','y inc',...
'Style','text');

function gdal_project_uiCB(hObject, eventdata, h1, callback_name)
% This function is executed by the callback and than the handles is allways updated.
	feval(callback_name,hObject,guidata(h1));
