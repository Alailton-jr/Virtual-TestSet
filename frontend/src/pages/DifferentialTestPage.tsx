import { useState } from 'react'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select'
import { Badge } from '@/components/ui/badge'
import { Play } from 'lucide-react'
import { useStreamStore } from '@/stores/useStreamStore'

interface TestPoint {
  id: number
  ir: number
  result: 'pass' | 'fail' | 'pending'
}

export default function DifferentialTestPage() {
  const { streams } = useStreamStore()
  const [side1StreamId, setSide1StreamId] = useState<string>('')
  const [side2StreamId, setSide2StreamId] = useState<string>('')
  const [slope, setSlope] = useState('25')
  const [restraint, setRestraint] = useState('0.3')
  const [testPoints, setTestPoints] = useState<TestPoint[]>([
    { id: 1, ir: 1.0, result: 'pending' },
    { id: 2, ir: 2.0, result: 'pending' },
    { id: 3, ir: 5.0, result: 'pending' },
  ])

  const handleRunTest = () => {
    setTestPoints(testPoints.map(p => ({ ...p, result: Math.random() > 0.3 ? 'pass' : 'fail' })))
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-3xl font-bold tracking-tight">Differential 87 Test</h1>
        <p className="text-muted-foreground">
          Automated testing for differential protection relays
        </p>
      </div>

      <div className="grid gap-6 lg:grid-cols-2">
        <Card>
          <CardHeader><CardTitle>Test Configuration</CardTitle><CardDescription>Define restraint and differential current points for testing</CardDescription></CardHeader>
          <CardContent className="space-y-4">
            <div className="grid gap-4 sm:grid-cols-2">
              <div className="space-y-2">
                <Label htmlFor="side1">Side 1 Stream</Label>
                <Select value={side1StreamId} onValueChange={setSide1StreamId}>
                  <SelectTrigger id="side1"><SelectValue placeholder="Select stream" /></SelectTrigger>
                  <SelectContent>{streams.map((s) => <SelectItem key={s.id} value={s.id}>{s.name}</SelectItem>)}</SelectContent>
                </Select>
              </div>
              <div className="space-y-2">
                <Label htmlFor="side2">Side 2 Stream</Label>
                <Select value={side2StreamId} onValueChange={setSide2StreamId}>
                  <SelectTrigger id="side2"><SelectValue placeholder="Select stream" /></SelectTrigger>
                  <SelectContent>{streams.map((s) => <SelectItem key={s.id} value={s.id}>{s.name}</SelectItem>)}</SelectContent>
                </Select>
              </div>
            </div>
            <div className="grid gap-4 sm:grid-cols-2">
              <div className="space-y-2">
                <Label htmlFor="slope">Slope (%)</Label>
                <Input id="slope" type="number" step="1" value={slope} onChange={(e) => setSlope(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="restraint">Min Restraint (A)</Label>
                <Input id="restraint" type="number" step="0.1" value={restraint} onChange={(e) => setRestraint(e.target.value)} />
              </div>
            </div>
            <Button onClick={handleRunTest} disabled={!side1StreamId || !side2StreamId} className="w-full">
              <Play className="mr-2 h-4 w-4" />Run Test
            </Button>
            <div className="space-y-2">
              <Label>Test Points</Label>
              {testPoints.map((pt) => (
                <div key={pt.id} className="flex items-center justify-between p-2 border rounded text-sm">
                  <span className="font-mono">Ir = {pt.ir.toFixed(1)} A</span>
                  <Badge variant={pt.result === 'pass' ? 'default' : pt.result === 'fail' ? 'destructive' : 'outline'}>{pt.result}</Badge>
                </div>
              ))}
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader><CardTitle>Id vs Ir Plot</CardTitle><CardDescription>Operating characteristic</CardDescription></CardHeader>
          <CardContent>
            <div className="h-96 flex items-center justify-center border rounded-lg bg-muted/20">
              <p className="text-sm text-muted-foreground">Differential characteristic plot coming soon...</p>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
