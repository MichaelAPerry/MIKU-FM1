// Headless recorder using the emulator's official shared live/browser execution backend.
use fm1_core::{Bus,Cpu};
use fm1_soc::{Soc,machine,RAM_BASE};
fn main(){
 let args:Vec<String>=std::env::args().collect();
 let package=std::fs::read(&args[1]).unwrap();
 let stock=fm1_soc::stock_from_package(&package,None).unwrap();
 let mut soc=Soc::new(1024*1024,stock.xip.clone());
 soc.periph.hz=240_000_000;soc.periph.cycle_ticks=1;soc.periph.skip_idle=true;
 let mut cpu=Cpu::new(stock.entry);cpu.jit_on=true;cpu.user=true;
 let params=RAM_BASE+0x7fc00;soc.write32(params,params+0x40);
 cpu.regs[0]=fm1_soc::stock_boot(&mut soc,&stock.flash,params,stock.board);
 soc.sfc_boot(stock.key,stock.app_end);
 soc.take_boot(&cpu);
 if let Ok(path)=std::env::var("FM1_FLASH_IN"){soc.restore_flash(&std::fs::read(path).unwrap());}
 fm1_soc::script::queue(&mut soc,&args[4]);
 let duration:u64=args[2].parse().unwrap();let mut cpu1=None;let mut tick=false;
 let mut capture=0u64;let start=std::time::Instant::now();
 while soc.periph.ms()<duration {
  machine::run_dual(&mut cpu1,&mut soc,100_000,1024*1024,true,|s,n|machine::run_blocks(&mut cpu,s,n,&mut tick)).unwrap();
  if args.len()>5&&soc.periph.ms()>=capture {
   fm1_soc::write_bmp(&format!("{}/frame_{:05}.bmp",args[5],capture/100),&soc.lcd.fb,soc.lcd.inverted).unwrap();capture+=100;
  }
 }
 write_wav(&args[3],&soc.periph.audio).unwrap();
 if let Ok(path)=std::env::var("FM1_FLASH_OUT"){std::fs::write(path,&soc.flash.mem).unwrap();}
 println!("{} emulated ms, {} stereo frames, {:.3} real seconds",soc.periph.ms(),soc.periph.audio.len()/2,start.elapsed().as_secs_f64());
}
fn write_wav(path: &str, samples: &[i16]) -> std::io::Result<()> {
    use std::io::Write;
    let data_len = (samples.len() * 2) as u32;
    let mut f = std::io::BufWriter::new(std::fs::File::create(path)?);
    f.write_all(b"RIFF")?;
    f.write_all(&(36 + data_len).to_le_bytes())?;
    f.write_all(b"WAVEfmt ")?;
    f.write_all(&16u32.to_le_bytes())?;
    f.write_all(&1u16.to_le_bytes())?; // PCM
    f.write_all(&2u16.to_le_bytes())?; // stereo
    f.write_all(&44100u32.to_le_bytes())?;
    f.write_all(&(44100u32 * 4).to_le_bytes())?;
    f.write_all(&4u16.to_le_bytes())?;
    f.write_all(&16u16.to_le_bytes())?;
    f.write_all(b"data")?;
    f.write_all(&data_len.to_le_bytes())?;
    for s in samples {
        f.write_all(&s.to_le_bytes())?;
    }
    Ok(())
}
