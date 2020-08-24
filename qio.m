classdef qio
   properties
      port
      host
      QS
      user
      pass
      ns
   end
   methods
      function obj = qio(h,p,u,pa,ns)
         if nargin >= 2
            if isnumeric(p)
               obj.port = p;
            else
               error('Value must be numeric')
            end
            obj.user=u;
            obj.host=h; 
            obj.ns=ns;
            obj.pass=pa;
            obj.QS=qsend(obj.host, obj.port,obj.user,obj.pass);
         end
      end
      
      function r = createTable(obj,tab,stor,cols,keys)
          r = obj.QS.Q('mat_createTable',obj.ns,tab,stor,cols,keys);
      end
      
      function r = setValue(obj,tab,key,dict)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_setValue',obj.ns,tab,key,dict);
      end
      
      function r = getValue(obj,tab,key)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_getValue',obj.ns,tab,key);
      end
      
      function r = reduce(obj,tab,func)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_reduce',obj.ns,tab,func);
      end
      
      function r = gather(obj,tab)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_gather',obj.ns,tab);
      end
      
      function r = query(obj,varargin)
         %dict.user_id=obj.user;
         r = obj.QS.Q(varargin{:});
      end
      
      function r = save(obj,tab)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_save',obj.ns,tab);
      end
      
      function r = load(obj,tab)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_load',obj.ns,tab);
      end
      
       function r = drop(obj,tab)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_drop',obj.ns,tab);
       end
       
       function r = setHetParamValue(obj,tab,key,param,dict)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_setHetParamValue',obj.ns,tab,key,param,dict);
      end

      function r = setHetValue(obj,tab,key,dict)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_setHetValue',obj.ns,tab,key,dict);
      end
      
      function r = getHetParamValue(obj,tab,key,param)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_getHetParamValue',obj.ns,tab,key,param);
      end
      
      function r = getHetValue(obj,tab,key)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_getHetValue',obj.ns,tab,key);
      end
      
      function r = hetParamReduce(obj,tab,param,func)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_hetParamReduce',obj.ns,tab,param,func);
      end
      
      function r = hetParamGather(obj,tab,param)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_hetParamGather',obj.ns,tab,param);
      end
       
      function r = struct2Row(obj,tab,dict)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_struct2row',obj.ns,tab,dict);
      end
      
      function r = struct2Cols(obj,tab,dict)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_struct2cols',obj.ns,tab,dict);
      end
      
      function r = struct2Table(obj,tab,key,dict)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_struct2table',obj.ns,tab,key,dict);
      end
      
      function r = table2Table(obj,tab,dict)
         %dict.user_id=obj.user;
         r = obj.QS.Q('mat_table2table',obj.ns,tab,dict);
      end
      
   end
end
