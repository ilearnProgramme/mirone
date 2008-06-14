function varargout = poly2mask_fig(varargin)
% Interface figure to create a mask image from polygons in the calling fig 
 
	hObject = figure('Tag','figure1','Visible','off');
	poly2mask_fig_LayoutFcn(hObject);
	handles = guihandles(hObject);
	movegui(hObject,'center')
 
	if (numel(varargin) > 0)
		handles.head = varargin{1}.head;
		handles.hMirAxes = varargin{1}.axes1;
		handles.hMirImg = varargin{1}.hImg;
		handles.image_type = varargin{1}.image_type;
		handles.hPoly_current = varargin{2};
	end

	handles.grid_in  = 1;
	handles.grid_out = 0;

	% Get handles of closed lines (lines & patches)
	handles.hPoly = find_closed(handles);
	if (numel(handles.hPoly) > 1)
		set(handles.check_allPolygs,'Vis','on')
	end
	
	set(hObject,'Visible','on');
	guidata(hObject, handles);
	if (nargout),	varargout{1} = 	hObject;	end

% -------------------------------------------------------------------------
function radio_binary_Callback(hObject, eventdata, handles)
	if (~get(hObject,'Value')),		set(hObject,'Value',1),		return,		end
	set(handles.radio_float,'Val',0)
	set([handles.edit_in handles.edit_out],'Enable','off')
	set([handles.radio_in handles.radio_out],'Enable','on')

% -------------------------------------------------------------------------
function radio_float_Callback(hObject, eventdata, handles)
	if (~get(hObject,'Value')),		set(hObject,'Value',1),		return,		end
	set(handles.radio_binary,'Val',0)
	set([handles.edit_in handles.edit_out],'Enable','on')
	set([handles.radio_in handles.radio_out],'Enable','off')

% -------------------------------------------------------------------------
function radio_in_Callback(hObject, eventdata, handles)
	if (~get(hObject,'Value')),		set(hObject,'Value',1),		return,		end
	set(handles.radio_out,'Val',0)

% -------------------------------------------------------------------------
function radio_out_Callback(hObject, eventdata, handles)
	if (~get(hObject,'Value')),		set(hObject,'Value',1),		return,		end
	set(handles.radio_in,'Val',0)

% -------------------------------------------------------------------------
function edit_in_Callback(hObject, eventdata, handles)
	handles.grid_in = str2double(get(hObject,'String'));
	guidata(handles.figure1, handles)

% -------------------------------------------------------------------------
function edit_out_Callback(hObject, eventdata, handles)
	handles.grid_out = str2double(get(hObject,'String'));
	guidata(handles.figure1, handles)

% -------------------------------------------------------------------------
function push_ok_Callback(hObject, eventdata, handles)
	
	if (get(handles.check_allPolygs,'Val'))			% We have more than one closed poly
		hLine = handles.hPoly;
	else
		hLine = handles.hPoly_current;
	end
	
	I = get(handles.hMirImg,'CData');
	limits = getappdata(handles.hMirAxes,'ThisImageLims');
	x = get(hLine(1),'XData');   y = get(hLine(1),'YData');
	mask = img_fun('roipoly_j',limits(1:2),limits(3:4),I,x,y);
	mask2 =  [];
	for (k = 2:numel(hLine))
		x = get(hLine(k),'XData');   y = get(hLine(k),'YData');
		mask2 = img_fun('roipoly_j',limits(1:2),limits(3:4),I,x,y);
		mask = mask | mask2;		% Combine all masks
	end
	if (~isempty(mask2)),	clear mask2;	end
	
	if (get(handles.radio_float,'Val'))
		Z = single(false(size(mask)));
		Z(mask) = single(handles.grid_in);
		Z(~mask) = single(handles.grid_out);
	else
		if (get(handles.radio_out,'Val')),		mask = ~mask;	end
		Z = mask;
	end
	
	if ( handles.image_type ~= 2 || get(handles.radio_float,'Val') )
		tmp.head = handles.head;	tmp.name = 'Mask from polygon';
		tmp.X = tmp.head(1:2);		tmp.Y = tmp.head(3:4);
		if (get(handles.radio_float,'Val'))
			tmp.X = linspace(tmp.X(1), tmp.X(2), size(Z,2));
			tmp.Y = linspace(tmp.Y(1), tmp.Y(2), size(Z,1));
		end
     	mirone(Z, tmp)
	else
		h = mirone(Z);
		set(h,'Name','Mask from polygon')
	end

% -------------------------------------------------------------------------
function hPoly = find_closed(handles)
% Get handles of closed lines (lines & patches)

	hPoly = findobj(handles.hMirAxes,'Type','line');
	% Find if any of the eventual above line is closed
	if (~isempty(hPoly))
		vec = false(numel(hPoly),1);
		for (k = 1:numel(hPoly))
			x = get(hPoly(k),'XData');   y = get(hPoly(k),'YData');
			if (numel(x) >= 3 && x(1) == x(end) && y(1) == y(end) )
				vec(k) = true;
			end
		end
		hPoly = hPoly(vec);
	end
	
	hPoly = [hPoly; findobj(handles.hMirAxes,'Type','patch')];
	hPoly = unique([handles.hPoly_current; hPoly]);		% one of them is repeated

% -------------------------------------------------------------------------
% --- Creates and returns a handle to the GUI figure. 
function poly2mask_fig_LayoutFcn(h1)

set(h1,'Color',get(0,'factoryUicontrolBackgroundColor'),...
'MenuBar','none',...
'Name','poly2mask',...
'NumberTitle','off',...
'Position',[520 697 271 90],...
'Resize','off',...
'HandleVisibility','callback',...
'Tag','figure1');

uicontrol('Parent',h1,...
'Callback',{@poly2mask_fig_uicallback,h1,'radio_in_Callback'},...
'FontName','Helvetica',...
'Position',[77 47 55 16],...
'String','Inside',...
'Style','radiobutton',...
'TooltipString','Polygon interior set to 1',...
'Value',1,...
'Tag','radio_in');

uicontrol('Parent',h1,...
'Callback',{@poly2mask_fig_uicallback,h1,'radio_out_Callback'},...
'FontName','Helvetica',...
'Position',[10 47 60 16],...
'String','Outside',...
'Style','radiobutton',...
'TooltipString','Outside polygon set to 0',...
'Tag','radio_out');

uicontrol('Parent',h1,...
'Callback',{@poly2mask_fig_uicallback,h1,'radio_binary_Callback'},...
'FontName','Helvetica',...
'Position',[39 70 85 15],...
'String','Binary Mask',...
'Style','radiobutton',...
'TooltipString','Create a black and white mask image',...
'Value',1,...
'Tag','radio_binary');

uicontrol('Parent',h1,...
'Callback',{@poly2mask_fig_uicallback,h1,'radio_float_Callback'},...
'FontName','Helvetica',...
'Position',[168 70 75 15],...
'String','Float Mask',...
'Style','radiobutton',...
'TooltipString','Create a float mask image',...
'Tag','radio_float');

uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Callback',{@poly2mask_fig_uicallback,h1,'edit_in_Callback'},...
'Enable','off',...
'Position',[199 44 51 21],...
'String','1.0',...
'Style','edit',...
'TooltipString','Inside polygon value. (NaNs are alowed)',...
'Tag','edit_in');

uicontrol('Parent',h1,...
'BackgroundColor',[1 1 1],...
'Callback',{@poly2mask_fig_uicallback,h1,'edit_out_Callback'},...
'Enable','off',...
'Position',[146 44 51 21],...
'String','0.0',...
'Style','edit',...
'TooltipString','Outside polygon value. (NaNs are alowed)',...
'Tag','edit_out');

uicontrol('Parent',h1,...
'FontName','Helvetica',...
'Position',[10 14 118 16],...
'String','Apply to all polygons',...
'Style','checkbox',...
'TooltipString','Apply settings to all polygons. If unchecked apply only to the selected one.',...
'Tag','check_allPolygs',...
'Visible','off');

uicontrol('Parent',h1,...
'Callback',{@poly2mask_fig_uicallback,h1,'push_ok_Callback'},...
'FontName','Helvetica',...
'FontSize',9,...
'FontWeight','bold',...
'Position',[200 5 50 21],...
'String','OK',...
'Tag','push_ok');

function poly2mask_fig_uicallback(hObject, eventdata, h1, callback_name)
% This function is executed by the callback and than the handles is allways updated.
feval(callback_name,hObject,[],guidata(h1));
