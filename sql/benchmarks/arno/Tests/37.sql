SELECT MODEL202.is_mutagen, count(distinct MODEL202.model_id ) FROM MODEL MODEL202, BOND T1008290413680  WHERE MODEL202.model_id=T1008290413680.model_id AND MODEL202.lumo='-2' group by MODEL202.is_mutagen;
